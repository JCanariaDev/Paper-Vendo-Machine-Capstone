#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Esp.h>
#include <Preferences.h>

// ==============================================================================
// REVAMPED ESP32 IOT GATEWAY FIRMWARE (PRODUCTION READY)
// Communicates with Arduino Mega 2560 over Serial2 and bridges to Supabase.
// Handles Dynamic 2-Bay Paper (database stock + exit confirmation) and 1-Bay Ballpen Vending.
// ==============================================================================

// --- WIFI CONFIG ---
// Bootstrap credentials are used only when no working credentials have been
// saved in ESP32 flash yet. Replace these with the initial machine network.
const char* BOOTSTRAP_WIFI_SSID = "ashid";
const char* BOOTSTRAP_WIFI_PASSWORD = "paltankolang";
String wifiSsid;
String wifiPassword;
String previousWifiSsid;
String previousWifiPassword;
Preferences wifiPreferences;
const char* NETWORK_CONFIG_URL = "https://paper-vendo-backend.onrender.com/api/machine/network-config/device";
const char* NETWORK_CONFIG_TOKEN = "Pv2C03l9X3ilSi9b3SkFhi9fc6mFz2Co3GbmGh1gWX4=";
String lastNetworkConfigVersion = "";
unsigned long lastNetworkConfigCheck = 0;
const unsigned long NETWORK_CONFIG_CHECK_INTERVAL = 30000;
const int MAX_BALLPENS_PER_TRANSACTION = 5;

// --- SUPABASE CONFIG ---
const char* SUPABASE_URL = "https://jowpzdynbdeznuvohrpx.supabase.co";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

HardwareSerial &MEGA_SERIAL = Serial2;
const int MEGA_RX_PIN = 16;
const int MEGA_TX_PIN = 17;

bool wifiConnected = false;
unsigned long lastHeartbeatAt = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 1000; // WIFI: status ping to Mega
unsigned long lastOnlineHeartbeatAt = 0;
const unsigned long ONLINE_HEARTBEAT_INTERVAL_MS = 1000; // Supabase heartbeat

// A finish request must survive a temporary Wi-Fi/API failure.  Keep the
// payload in RAM and retry it from loop() instead of leaving the Mega waiting.
bool pendingFinish = false;
String pendingFinishTransactionId;
String pendingFinishResults;
int pendingFinishChangePaidCents = 0;
unsigned long nextFinishRetryAt = 0;
uint8_t finishRetryCount = 0;
const unsigned long FINISH_RETRY_INTERVAL_MS = 3000;
const uint8_t MAX_FINISH_RETRIES = 20;

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // machine_status table update

unsigned long lastWiFiCheck = 0;
unsigned long disconnectedSince = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long WIFI_STUCK_THRESHOLD = 20000;

bool connectToWifi(unsigned long timeoutMs);
bool printNearbyWifiNetworks();
void updateMachineStatus();
void updateStatusKey(const String &key, const String &value);
bool sendOnlineHeartbeat();
void softResetRuntime();
bool fetchAndApplyRemoteNetworkConfig();
bool acknowledgeRemoteNetworkConfig(const String &version);
bool submitFinishTransaction(const String &transactionId,
                             const String &encodedResults,
                             int changePaidCents,
                             String &trNumber,
                             String &status,
                             int &dueCents,
                             int &paidCents);
void processPendingFinish();
void loadSavedWifiCredentials();
void saveWifiCredentials(const String &ssid, const String &password,
                         const String &previousSsid, const String &previousPassword,
                         const String &version);
bool connectUsingSavedFallbacks();

void sendError(const String &message) {
  MEGA_SERIAL.println("ERR:" + message);
}

void sendWifiStatus() {
  MEGA_SERIAL.println("WIFI:" + String(wifiConnected ? 1 : 0));
}

bool ensureWifi() {
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  return wifiConnected;
}

bool connectToWifi(unsigned long timeoutMs) {
  static bool wifiEverStarted = false;
  Serial.println("Initializing WiFi...");

  if (wifiEverStarted) {
    WiFi.disconnect(true, false);
    delay(300);
  }
  wifiEverStarted = true;

  WiFi.mode(WIFI_OFF);
  delay(100);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  delay(250);

  Serial.print("ESP32 MAC: ");
  Serial.println(WiFi.macAddress());
  bool targetFound = printNearbyWifiNetworks();

  MEGA_SERIAL.println(targetFound ? "WIFISTATE:CONNECTING" : "WIFISTATE:NOTFOUND");

  Serial.println("Starting WiFi connection...");
  WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! Machine Online.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    return true;
  } else {
    Serial.println("\nWiFi connection timed out. Continuing offline.");
    wifiConnected = false;
    return false;
  }
}

bool printNearbyWifiNetworks() {
  Serial.println("Scanning WiFi networks...");
  int networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) return false;

  bool targetFound = false;
  for (int i = 0; i < networkCount; i++) {
    if (WiFi.SSID(i) == wifiSsid) targetFound = true;
  }
  WiFi.scanDelete();
  return targetFound;
}

void loadSavedWifiCredentials() {
  wifiPreferences.begin("wifi-config", false);
  wifiSsid = wifiPreferences.getString("ssid", BOOTSTRAP_WIFI_SSID);
  wifiPassword = wifiPreferences.getString("password", BOOTSTRAP_WIFI_PASSWORD);
  previousWifiSsid = wifiPreferences.getString("prev_ssid", "");
  previousWifiPassword = wifiPreferences.getString("prev_password", "");
  lastNetworkConfigVersion = wifiPreferences.getString("version", "");

  Serial.print("WiFi credential source: ");
  Serial.println(wifiPreferences.isKey("ssid") ? "ESP32 flash" : "bootstrap firmware");
}

void saveWifiCredentials(const String &ssid, const String &password,
                         const String &previousSsid, const String &previousPassword,
                         const String &version) {
  wifiPreferences.putString("ssid", ssid);
  wifiPreferences.putString("password", password);
  wifiPreferences.putString("prev_ssid", previousSsid);
  wifiPreferences.putString("prev_password", previousPassword);
  wifiPreferences.putString("version", version);
}

bool connectUsingSavedFallbacks() {
  const String activeSsid = wifiSsid;
  const String activePassword = wifiPassword;

  if (connectToWifi(15000)) return true;

  if (previousWifiSsid.length() > 0 &&
      (previousWifiSsid != activeSsid || previousWifiPassword != activePassword)) {
    Serial.println("Saved WiFi failed. Trying previous known-good credentials...");
    wifiSsid = previousWifiSsid;
    wifiPassword = previousWifiPassword;
    if (connectToWifi(15000)) {
      saveWifiCredentials(wifiSsid, wifiPassword, activeSsid, activePassword, "");
      lastNetworkConfigVersion = "";
      return true;
    }
  }

  if (activeSsid != BOOTSTRAP_WIFI_SSID || activePassword != BOOTSTRAP_WIFI_PASSWORD) {
    Serial.println("Saved WiFi fallback failed. Trying bootstrap credentials...");
    wifiSsid = BOOTSTRAP_WIFI_SSID;
    wifiPassword = BOOTSTRAP_WIFI_PASSWORD;
    if (connectToWifi(15000)) {
      saveWifiCredentials(wifiSsid, wifiPassword, activeSsid, activePassword, "");
      lastNetworkConfigVersion = "";
      return true;
    }
  }

  wifiSsid = activeSsid;
  wifiPassword = activePassword;
  return false;
}

bool acknowledgeRemoteNetworkConfig(const String &version) {
  if (version.length() == 0) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, String(NETWORK_CONFIG_URL) + "/ack")) return false;
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Device-Token", NETWORK_CONFIG_TOKEN);
  DynamicJsonDocument request(256);
  request["version"] = version;
  String body;
  serializeJson(request, body);
  const int code = http.POST(body);
  const String response = http.getString();
  Serial.printf("WiFi config acknowledgment response: %d\n", code);
  if (code < 200 || code >= 300) {
    Serial.println(response);
  }
  http.end();
  return code >= 200 && code < 300;
}

bool fetchAndApplyRemoteNetworkConfig() {
  if (!ensureWifi()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, NETWORK_CONFIG_URL)) {
    Serial.println("Remote WiFi config request could not start.");
    return false;
  }
  http.setTimeout(10000);
  http.addHeader("X-Device-Token", NETWORK_CONFIG_TOKEN);
  const int code = http.GET();
  const String payload = http.getString();
  http.end();

  if (code == 404) return false; // No staged configuration yet.
  if (code < 200 || code >= 300) {
    Serial.printf("Remote WiFi config request failed: %d\n", code);
    return false;
  }

  DynamicJsonDocument response(1536);
  if (deserializeJson(response, payload)) {
    Serial.println("Remote WiFi config response was invalid.");
    return false;
  }

  JsonObject config = response["config"];
  if (config.isNull()) return false;
  const String version = config["updated_at"].as<String>();
  const String candidateSsid = config["ssid"].as<String>();
  const String candidatePassword = config["password"].as<String>();
  if (version.length() == 0 || candidateSsid.length() == 0 || candidatePassword.length() < 8) {
    Serial.println("Remote WiFi config was incomplete.");
    return false;
  }
  if (version == lastNetworkConfigVersion) return true;

  if (candidateSsid == wifiSsid && candidatePassword == wifiPassword) {
    // Do not mark the version as applied until the backend confirms it.
    // This makes an interrupted acknowledgment retry on the next poll.
    if (acknowledgeRemoteNetworkConfig(version)) {
      lastNetworkConfigVersion = version;
      saveWifiCredentials(wifiSsid, wifiPassword, previousWifiSsid, previousWifiPassword, version);
      Serial.println("Remote WiFi config already matches the active credentials.");
      return true;
    }
    Serial.println("Remote WiFi config matches locally, but backend acknowledgment failed.");
    return false;
  }

  const String previousSsid = wifiSsid;
  const String previousPassword = wifiPassword;
  wifiSsid = candidateSsid;
  wifiPassword = candidatePassword;

  Serial.print("Applying remote WiFi configuration for SSID: ");
  Serial.println(wifiSsid);
  if (connectToWifi(15000)) {
    previousWifiSsid = previousSsid;
    previousWifiPassword = previousPassword;
    // Persist the working credentials immediately, but leave the version blank
    // until the backend acknowledges the exact configuration revision.
    saveWifiCredentials(wifiSsid, wifiPassword, previousWifiSsid, previousWifiPassword, "");
    if (acknowledgeRemoteNetworkConfig(version)) {
      lastNetworkConfigVersion = version;
      saveWifiCredentials(wifiSsid, wifiPassword, previousWifiSsid, previousWifiPassword, version);
      Serial.println("Remote WiFi configuration applied and acknowledged successfully.");
      return true;
    }
    Serial.println("Remote WiFi connected, but backend acknowledgment failed. Will retry.");
    return false;
  }

  Serial.println("Remote WiFi configuration failed. Restoring previous credentials.");
  wifiSsid = previousSsid;
  wifiPassword = previousPassword;
  connectToWifi(15000);
  return false;
}

void updateMachineStatus() {
  if (WiFi.status() != WL_CONNECTED) return;
  long rssi = WiFi.RSSI();
  String strength = rssi >= -50 ? "Excellent" : (rssi >= -60 ? "Good" : (rssi >= -70 ? "Fair" : "Poor"));
  updateStatusKey("is_running", "Online");
  updateStatusKey("wifi_signal", strength + " (" + String(rssi) + " dBm)");
}

void updateStatusKey(const String &key, const String &value) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  // Upsert lets the gateway publish new runtime keys without a manual SQL seed.
  String url = String(SUPABASE_URL) + "/rest/v1/machine_status?on_conflict=status_key";

  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "resolution=merge-duplicates,return=minimal");
    String body = "{\"status_key\":\"" + key + "\",\"status_value\":\"" + value + "\",\"updated_at\":\"now()\"}";
    int code = http.POST(body);
    if (code < 200 || code >= 300) {
      Serial.printf("Status update failed for %s: %d\n", key.c_str(), code);
    }
    http.end();
  }

}

void handleCreditUpdate(String message) {
  int separator = message.indexOf(':');
  if (separator < 0) return;
  int credits = message.substring(separator + 1).toInt();
  if (credits < 0) return;
  updateStatusKey("current_credits", String(credits));
}

bool sendOnlineHeartbeat() {
  if (!ensureWifi()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String(SUPABASE_URL) + "/rest/v1/machine_online_status?id=eq.1";
  if (!http.begin(client, url)) {
    Serial.println("Online heartbeat request could not start.");
    return false;
  }

  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");

  // The Supabase trigger updates last_heartbeat and updated_at with server time.
  const int code = http.PATCH("{\"status\":\"Online\"}");
  http.end();

  if (code < 200 || code >= 300) {
    Serial.printf("Online heartbeat failed: HTTP %d\n", code);
    return false;
  }
  return true;
}

bool callRpc(const char* functionName, JsonDocument &request, DynamicJsonDocument &response) {
  if (!ensureWifi()) {
    sendError("WIFI_OFFLINE");
    return false;
  }
  String body;
  serializeJson(request, body);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String(SUPABASE_URL) + "/rest/v1/rpc/" + functionName;
  if (!http.begin(client, url)) {
    sendError("HTTPS_START_FAILED");
    return false;
  }
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(body);
  const String payload = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    Serial.printf("RPC %s failed: %d %s\n", functionName, code, payload.c_str());
    // Forward the database's actual reason to the Mega instead of hiding it
    // behind a generic error. This is especially useful for paper stock and
    // compartment assignment failures.
    String reason = "HTTP_" + String(code);
    DynamicJsonDocument errorDoc(768);
    if (deserializeJson(errorDoc, payload) == DeserializationError::Ok &&
        !errorDoc["message"].isNull()) {
      reason = errorDoc["message"].as<String>();
    }
    reason.replace(':', '-');
    reason.replace('\n', ' ');
    if (reason.length() > 90) reason = reason.substring(0, 90);
    sendError("DATABASE_REJECTED:" + reason);
    return false;
  }
  if (payload.length() == 0) {
    response.clear();
    return true;
  }
  if (deserializeJson(response, payload)) {
    sendError("DATABASE_RESPONSE_INVALID");
    return false;
  }
  return true;
}

bool getTransactionPlan(const String &transactionId, DynamicJsonDocument &response) {
  if (!ensureWifi()) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String(SUPABASE_URL) + "/rest/v1/sales_transaction_lines?transaction_id=eq." + transactionId + "&select=item_type,product_id,physical_channel,qty_requested";
  if (!http.begin(client, url)) return false;
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  const int code = http.GET();
  const String payload = http.getString();
  http.end();
  return code == 200 && !deserializeJson(response, payload);
}

// Fetches live 2 Paper Bay assignments & 1 Pen Bay assignment for the Mega's dynamic catalog UI
void syncLiveCatalogToMega() {
  if (!ensureWifi()) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // 1. Fetch Paper Compartments
  String url = String(SUPABASE_URL) + "/rest/v1/paper_compartments?select=compartment_number,assigned_product_id,presence_status,current_pad_stock,paper_inventory(brand_name,paper_size,sheets_per_unit,cost_per_unit_cents)&compartment_number=lte.2&order=compartment_number.asc";
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, http.getString());
      for (JsonObject bay : doc.as<JsonArray>()) {
        int bayNum = bay["compartment_number"];
        int prodId = bay["assigned_product_id"] | 0;
        String presence = bay["presence_status"].as<String>();
        int padStock = bay["current_pad_stock"] | 0;
        JsonObject inv = bay["paper_inventory"];
        String brand = inv["brand_name"].as<String>();
        String size = inv["paper_size"].as<String>();
        int sheets = inv["sheets_per_unit"] | 1;
        int price = inv["cost_per_unit_cents"] | 100;
        // Format: PAPER_BAY:<bay>:<product>:<legacy_presence>:<sheets_per_pad>:<pad_stock>:<price_cents>:<name>
        MEGA_SERIAL.println("PAPER_BAY:" + String(bayNum) + ":" + String(prodId) + ":" + presence + ":" + String(sheets) + ":" + String(padStock) + ":" + String(price) + ":" + brand + " " + size);
        delay(30);
      }
    }
    http.end();
  }

  // 2. Fetch Pen Compartments
  url = String(SUPABASE_URL) + "/rest/v1/ballpen_compartments?select=compartment_number,assigned_product_id,current_piece_stock,ballpen_inventory(item_name,cost_per_unit_cents)&compartment_number=lte.1&order=compartment_number.asc";
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getString());
      for (JsonObject bay : doc.as<JsonArray>()) {
        int bayNum = bay["compartment_number"];
        int prodId = bay["assigned_product_id"] | 0;
        int stock = bay["current_piece_stock"] | 0;
        JsonObject inv = bay["ballpen_inventory"];
        String name = inv["item_name"].as<String>();
        int price = inv["cost_per_unit_cents"] | 500;
        // Format: PEN_BAY:<bay_num>:<prod_id>:<stock>:<price_cents>:<name>
        MEGA_SERIAL.println("PEN_BAY:" + String(bayNum) + ":" + String(prodId) + ":" + String(stock) + ":" + String(price) + ":" + name);
        delay(30);
      }
    }
    http.end();
  }

  // 3. Fetch machine-wide operating options for the Mega.
  url = String(SUPABASE_URL) + "/rest/v1/machine_options?id=eq.1&select=minimum_credits,maximum_credits,minimum_ballpen_stock";
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument optionsDoc(512);
      if (deserializeJson(optionsDoc, http.getString()) == DeserializationError::Ok &&
          optionsDoc.as<JsonArray>().size() > 0) {
        JsonObject options = optionsDoc[0];
        MEGA_SERIAL.println("OPTIONS:" + String(options["minimum_credits"] | 1) + ":" +
                            String(options["maximum_credits"] | 30) + ":" +
                            String(options["minimum_ballpen_stock"] | 5));
      }
    }
    http.end();
  }
}

bool parseCartLine(const String &encoded, JsonArray lines) {
  const int first = encoded.indexOf(',');
  const int second = encoded.indexOf(',', first + 1);
  if (first <= 0 || second <= first + 1) return false;
  const String type = encoded.substring(0, first);
  const int productId = encoded.substring(first + 1, second).toInt();
  const int units = encoded.substring(second + 1).toInt();
  if ((type != "paper" && type != "pen") || productId <= 0 || units <= 0) return false;
  JsonObject line = lines.add<JsonObject>();
  line["item_type"] = type;
  line["product_id"] = productId;
  line["units"] = units;
  return true;
}

void reserveCart(const String &message) {
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) { sendError("BAD_RESERVE_FORMAT"); return; }
  const int creditCents = message.substring(first + 1, second).toInt();
  const String encodedLines = message.substring(second + 1);
  DynamicJsonDocument request(2048);
  request["p_credit_cents"] = creditCents;
  JsonArray lines = request.createNestedArray("p_lines");
  int ballpenUnits = 0;
  int start = 0;
  while (start < encodedLines.length()) {
    const int end = encodedLines.indexOf(';', start);
    const String encoded = end < 0 ? encodedLines.substring(start) : encodedLines.substring(start, end);
    if (!parseCartLine(encoded, lines)) { sendError("BAD_CART_LINE"); return; }
    const int comma1 = encoded.indexOf(',');
    const int comma2 = encoded.indexOf(',', comma1 + 1);
    if (encoded.substring(0, comma1) == "pen") {
      ballpenUnits += encoded.substring(comma2 + 1).toInt();
      if (ballpenUnits > MAX_BALLPENS_PER_TRANSACTION) {
        sendError("MAX_5_BALLPENS");
        return;
      }
    }
    if (end < 0) break;
    start = end + 1;
  }
  DynamicJsonDocument response(4096);
  if (!callRpc("machine_reserve_transaction", request, response)) return;
  JsonObject result = response[0];
  if (result.isNull()) { sendError("EMPTY_RESERVATION"); return; }

  String txId = result["transaction_id"].as<String>();
  String trNumber = result["tr_number"].as<String>();
  if (trNumber.length() == 0) trNumber = "TR-00000";
  int subtotalCents = result["subtotal_cents"] | 0;
  int changeDueCents = result["change_due_cents"] | 0;

  // Build encoded dispense plan directly from RPC response
  String encodedPlan;
  JsonArray planArray = result["dispense_plan"].as<JsonArray>();
  for (JsonObject line : planArray) {
    if (encodedPlan.length()) encodedPlan += ';';
    encodedPlan += line["item_type"].as<String>() + "," + String(line["product_id"].as<int>()) + "," + String(line["physical_channel"].as<int>()) + "," + String(line["qty_requested"].as<int>());
  }

  // Send complete Plan + TR Number directly to Mega (Product-First Flow)
  // Format: PLAN:<tx_id>:<tr_number>:<subtotal_cents>:<change_due_cents>:<encodedPlan>
  MEGA_SERIAL.println("PLAN:" + txId + ":" + trNumber + ":" + String(subtotalCents) + ":" + String(changeDueCents) + ":" + encodedPlan);
}

void changePaid(const String &message) {
  // Deprecated in Product-First flow, kept for compatibility
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) return;
  const String transactionId = message.substring(first + 1, second);
  const int paidCents = message.substring(second + 1).toInt();
  DynamicJsonDocument request(512), response(512);
  request["p_transaction_id"] = transactionId;
  request["p_change_paid_cents"] = paidCents;
  callRpc("machine_mark_change_paid", request, response);
}

void cancelReservation(const String &message) {
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) return;
  DynamicJsonDocument request(512), response(512);
  request["p_transaction_id"] = message.substring(first + 1, second);
  request["p_reason"] = message.substring(second + 1);
  callRpc("machine_cancel_reserved_transaction", request, response);
}

bool submitFinishTransaction(const String &transactionId,
                             const String &encodedResults,
                             int changePaidCents,
                             String &trNumber,
                             String &status,
                             int &dueCents,
                             int &paidCents) {
  DynamicJsonDocument request(2048), response(1024);
  request["p_transaction_id"] = transactionId;
  request["p_change_paid_cents"] = changePaidCents;
  JsonArray results = request.createNestedArray("p_results");

  int start = 0;
  while (start < encodedResults.length()) {
    const int end = encodedResults.indexOf(';', start);
    const String encoded = end < 0 ? encodedResults.substring(start) : encodedResults.substring(start, end);
    const int one = encoded.indexOf(',');
    const int two = encoded.indexOf(',', one + 1);
    if (one <= 0 || two <= one + 1) return false;

    JsonObject result = results.add<JsonObject>();
    result["item_type"] = encoded.substring(0, one);
    result["product_id"] = encoded.substring(one + 1, two).toInt();
    result["qty_dispensed"] = encoded.substring(two + 1).toInt();
    if (end < 0) break;
    start = end + 1;
  }

  if (!callRpc("machine_finish_transaction", request, response)) return false;

  JsonObject res = response[0];
  if (res.isNull()) return false;
  trNumber = res["tr_number"] | "TR-00000";
  status = res["final_status"] | "COMPLETED";
  dueCents = res["change_due_cents"] | 0;
  paidCents = res["change_paid_cents"] | 0;
  return true;
}

void processPendingFinish() {
  if (!pendingFinish || millis() < nextFinishRetryAt) return;

  finishRetryCount++;
  if (!ensureWifi()) {
    nextFinishRetryAt = millis() + FINISH_RETRY_INTERVAL_MS;
    return;
  }

  String trNumber;
  String status;
  int dueCents = 0;
  int paidCents = 0;

  Serial.printf("Submitting transaction completion (attempt %u/%u)\n",
                finishRetryCount, MAX_FINISH_RETRIES);
  if (submitFinishTransaction(pendingFinishTransactionId,
                              pendingFinishResults,
                              pendingFinishChangePaidCents,
                              trNumber,
                              status,
                              dueCents,
                              paidCents)) {
    MEGA_SERIAL.println("FINISHED:" + pendingFinishTransactionId + ":" +
                       trNumber + ":" + status + ":" +
                       String(dueCents) + ":" + String(paidCents));
    pendingFinish = false;
    finishRetryCount = 0;
    nextFinishRetryAt = 0;
    return;
  }

  if (finishRetryCount == 1) sendError("FINISH_PENDING");
  if (finishRetryCount >= MAX_FINISH_RETRIES) {
    sendError("FINISH_RETRY_FAILED");
    pendingFinish = false;
    finishRetryCount = 0;
    nextFinishRetryAt = 0;
    return;
  }

  nextFinishRetryAt = millis() + FINISH_RETRY_INTERVAL_MS;
}

void finishTransaction(const String &message) {
  // Format from Mega: FINISH:<tx_id>:<encodedResults>:<change_paid_cents>
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) { sendError("BAD_FINISH_FORMAT"); return; }
  const String transactionId = message.substring(first + 1, second);

  int third = message.indexOf(':', second + 1);
  String encodedResults;
  int changePaidCents = 0;
  if (third > 0) {
    encodedResults = message.substring(second + 1, third);
    changePaidCents = message.substring(third + 1).toInt();
  } else {
    encodedResults = message.substring(second + 1);
  }

  pendingFinish = true;
  pendingFinishTransactionId = transactionId;
  pendingFinishResults = encodedResults;
  pendingFinishChangePaidCents = changePaidCents;
  finishRetryCount = 0;
  nextFinishRetryAt = 0;
  processPendingFinish();
}

// Mark a bay empty when the Mega reports that its exit-verified stock is exhausted.
void updatePaperBayPresence(const String &message) {
  // Format: BAY_EMPTY:<bay_num>
  int bayNum = message.substring(10).toInt();
  if (bayNum < 1 || bayNum > 2) return;
  if (!ensureWifi()) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/paper_compartments?compartment_number=eq." + String(bayNum);
  if (http.begin(client, url)) {
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type", "application/json");
    String body = "{\"presence_status\":\"LOW\", \"current_pad_stock\":0, \"updated_at\":\"now()\"}";
    http.PATCH(body);
    http.end();
  }
}

void handleMegaMessage(String message) {
  message.trim();
  if (message.startsWith("CREDIT:")) handleCreditUpdate(message);
  else if (message.startsWith("RESERVE:")) reserveCart(message);
  else if (message.startsWith("CHANGE_OK:")) changePaid(message);
  else if (message.startsWith("CHANGE_FAIL:")) cancelReservation(message);
  else if (message.startsWith("FINISH:")) finishTransaction(message);
  else if (message.startsWith("BAY_EMPTY:")) updatePaperBayPresence(message);
  else if (message == "GET_CATALOG") syncLiveCatalogToMega();
  else if (message == "STATUS?") sendWifiStatus();
  else if (message == "SOFT_RESET") softResetRuntime();
  else if (message == "ESP_RESET") {
    MEGA_SERIAL.println("ERR:ESP_RESTARTING");
    delay(250);
    ESP.restart();
  }
}

void softResetRuntime() {
  wifiConnected = false;
  lastHeartbeatAt = 0;
  lastStatusUpdate = 0;
  lastWiFiCheck = 0;
  disconnectedSince = 0;
  sendWifiStatus();
  wifiConnected = connectToWifi(10000);
  sendWifiStatus();
  if (wifiConnected) {
    sendOnlineHeartbeat();
    updateMachineStatus();
    syncLiveCatalogToMega();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- REVAMPED ESP32 CLOUD GATEWAY STARTING ---");

  MEGA_SERIAL.begin(9600, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  loadSavedWifiCredentials();
  wifiConnected = connectUsingSavedFallbacks();
  sendWifiStatus();
  if (wifiConnected) {
    fetchAndApplyRemoteNetworkConfig();
    sendOnlineHeartbeat();
    updateMachineStatus();
    syncLiveCatalogToMega();
  }
}

void loop() {
  if (MEGA_SERIAL.available()) {
    handleMegaMessage(MEGA_SERIAL.readStringUntil('\n'));
  }

  ensureWifi();

  processPendingFinish();

  if (millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    sendWifiStatus();
  }

  if (wifiConnected && millis() - lastOnlineHeartbeatAt >= ONLINE_HEARTBEAT_INTERVAL_MS) {
    lastOnlineHeartbeatAt = millis();
    sendOnlineHeartbeat();
  }

  if (wifiConnected && millis() - lastStatusUpdate > statusInterval) {
    updateMachineStatus();
    syncLiveCatalogToMega();
    lastStatusUpdate = millis();
  }

  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    bool nowConnected = WiFi.status() == WL_CONNECTED;

    if (nowConnected != wifiConnected) {
      wifiConnected = nowConnected;
      sendWifiStatus();
      if (wifiConnected) fetchAndApplyRemoteNetworkConfig();
    }

    if (!nowConnected) {
      if (disconnectedSince == 0) {
        disconnectedSince = millis();
      } else if (millis() - disconnectedSince > WIFI_STUCK_THRESHOLD) {
        wifiConnected = connectToWifi(10000);
        sendWifiStatus();
        if (wifiConnected) sendOnlineHeartbeat();
        disconnectedSince = 0;
      }
    } else {
      disconnectedSince = 0;
    }
  }

  if (wifiConnected && millis() - lastNetworkConfigCheck >= NETWORK_CONFIG_CHECK_INTERVAL) {
    lastNetworkConfigCheck = millis();
    fetchAndApplyRemoteNetworkConfig();
  }
}

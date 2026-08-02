#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// --- WIFI CONFIG ---
const char* WIFI_SSID = "ashid";
const char* WIFI_PASSWORD = "paltankolang";

// --- SUPABASE CONFIG ---
const char* SUPABASE_URL = "https://jowpzdynbdeznuvohrpx.supabase.co";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

HardwareSerial &MEGA_SERIAL = Serial2;
const int MEGA_RX_PIN = 16;
const int MEGA_TX_PIN = 17;

bool wifiConnected = false;
unsigned long lastHeartbeatAt = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000; // WIFI: status ping to the Mega

// --- PORTED FROM Cloud_remake5: Supabase health heartbeat ---
unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // machine_status table update

// --- PORTED FROM Cloud_remake5: non-blocking reconnect watchdog ---
unsigned long lastWiFiCheck = 0;
unsigned long disconnectedSince = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long WIFI_STUCK_THRESHOLD = 20000;

bool connectToWifi(unsigned long timeoutMs);
bool printNearbyWifiNetworks();
void updateMachineStatus();
void updateStatusKey(const String &key, const String &value);

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

// ================= PORTED FROM Cloud_remake5 =================
// The full connect sequence, including the WIFI_OFF->STA fix, verbose
// debug output, and the WIFISTATE: messages the Mega's spinner/"can't
// be detected" UI depends on.

bool connectToWifi(unsigned long timeoutMs) {
  static bool wifiEverStarted = false;
  Serial.println("Initializing WiFi...");

  if (wifiEverStarted) {
    // Only tear the driver down if it was actually running before -
    // calling this on a never-initialized driver is what originally
    // triggered the xEventGroupSetBits assert on first boot.
    WiFi.disconnect(true, false);
    delay(300);
  }
  wifiEverStarted = true;

  WiFi.mode(WIFI_OFF);
  delay(100);

  WiFi.mode(WIFI_STA);
  Serial.println("  [ok] WiFi.mode(WIFI_STA)");
  WiFi.setSleep(false);
  Serial.println("  [ok] WiFi.setSleep(false)");
  WiFi.setAutoReconnect(true);
  Serial.println("  [ok] WiFi.setAutoReconnect(true)");
  WiFi.persistent(false);
  Serial.println("  [ok] WiFi.persistent(false)");
  delay(250);

  Serial.print("ESP32 MAC: ");
  Serial.println(WiFi.macAddress());
  bool targetFound = printNearbyWifiNetworks();

  // Tells the Mega which phase we're in, so it can show a spinner vs.
  // "WiFi can't be detected" instead of one generic "not connected".
  MEGA_SERIAL.println(targetFound ? "WIFISTATE:CONNECTING" : "WIFISTATE:NOTFOUND");

  Serial.println("Starting WiFi connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < timeoutMs) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected! Machine Online.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    wifiConnected = true;
    return true;
  } else {
    Serial.println("\nWiFi connection timed out. Continuing offline.");
    Serial.print("WiFi status code: ");
    Serial.println((int)WiFi.status());
    Serial.println("Mega serial communication remains available.");
    wifiConnected = false;
    return false;
  }
}

bool printNearbyWifiNetworks() {
  Serial.println("Scanning WiFi networks...");
  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0) {
    Serial.println("No WiFi networks found.");
    return false;
  }

  bool targetFound = false;
  Serial.print("Networks found: ");
  Serial.println(networkCount);

  for (int i = 0; i < networkCount; i++) {
    String foundSsid = WiFi.SSID(i);
    if (foundSsid == WIFI_SSID) {
      targetFound = true;
    }
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(foundSsid);
    Serial.print(" | RSSI ");
    Serial.print(WiFi.RSSI(i));
    Serial.print(" | Encryption ");
    Serial.println((int)WiFi.encryptionType(i));
  }

  Serial.print("Target SSID visible: ");
  Serial.println(targetFound ? "YES" : "NO");
  WiFi.scanDelete();
  return targetFound;
}

// Posts machine health to Supabase every 60s - dropped entirely in the
// newer file; restored so any webapp/admin dashboard reading the
// machine_status table doesn't go dark.
void updateMachineStatus() {
  if (WiFi.status() != WL_CONNECTED) return;

  long rssi = WiFi.RSSI();
  String strength;
  if (rssi >= -50) strength = "Excellent";
  else if (rssi >= -60) strength = "Good";
  else if (rssi >= -70) strength = "Fair";
  else strength = "Poor";

  updateStatusKey("is_running", "Online");
  updateStatusKey("wifi_signal", strength + " (" + String(rssi) + " dBm)");
}

void updateStatusKey(const String &key, const String &value) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/machine_status?status_key=eq." + key;

  http.begin(client, url);
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"status_value\":\"" + value + "\", \"updated_at\":\"now()\"}";
  http.PATCH(body);
  http.end();
}

// ================= KEPT FROM production_esp32 (unchanged logic) =================

bool callRpc(const char* functionName, JsonDocument &request, DynamicJsonDocument &response) {
  if (!ensureWifi()) {
    sendError("WIFI_OFFLINE");
    return false;
  }
  String body;
  serializeJson(request, body);
  WiFiClientSecure client;
  client.setInsecure(); // Replace with a pinned CA certificate before public deployment.
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
    sendError("DATABASE_REJECTED");
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
  // qty_requested is the physical output: sheets for paper and pieces for pens.
  // units_requested is deliberately not used here because one paper unit may be
  // a bundle of several sheets.
  const String url = String(SUPABASE_URL) + "/rest/v1/sales_transaction_lines?transaction_id=eq." + transactionId + "&select=item_type,product_id,physical_channel,qty_requested";
  if (!http.begin(client, url)) return false;
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  const int code = http.GET();
  const String payload = http.getString();
  http.end();
  return code == 200 && !deserializeJson(response, payload);
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
  int start = 0;
  while (start < encodedLines.length()) {
    const int end = encodedLines.indexOf(';', start);
    const String encoded = end < 0 ? encodedLines.substring(start) : encodedLines.substring(start, end);
    if (!parseCartLine(encoded, lines)) { sendError("BAD_CART_LINE"); return; }
    if (end < 0) break;
    start = end + 1;
  }
  DynamicJsonDocument response(4096);
  if (!callRpc("machine_reserve_transaction", request, response)) return;
  JsonObject result = response[0];
  if (result.isNull()) { sendError("EMPTY_RESERVATION"); return; }
  MEGA_SERIAL.println("RESERVED:" + result["transaction_id"].as<String>() + ":" + String(result["subtotal_cents"].as<int>()) + ":" + String(result["change_due_cents"].as<int>()));
}

void changePaid(const String &message) {
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) { sendError("BAD_CHANGE_CONFIRMATION"); return; }
  const String transactionId = message.substring(first + 1, second);
  const int paidCents = message.substring(second + 1).toInt();
  DynamicJsonDocument request(512), response(512);
  request["p_transaction_id"] = transactionId;
  request["p_change_paid_cents"] = paidCents;
  if (!callRpc("machine_mark_change_paid", request, response)) return;

  DynamicJsonDocument plan(2048);
  if (!getTransactionPlan(transactionId, plan)) { sendError("PLAN_UNAVAILABLE"); return; }
  String encodedPlan;
  for (JsonObject line : plan.as<JsonArray>()) {
    if (encodedPlan.length()) encodedPlan += ';';
    encodedPlan += line["item_type"].as<String>() + "," + String(line["product_id"].as<int>()) + "," + String(line["physical_channel"].as<int>()) + "," + String(line["qty_requested"].as<int>());
  }
  MEGA_SERIAL.println("PLAN:" + transactionId + ":" + encodedPlan);
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

void finishTransaction(const String &message) {
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) { sendError("BAD_FINISH_FORMAT"); return; }
  DynamicJsonDocument request(2048), response(512);
  request["p_transaction_id"] = message.substring(first + 1, second);
  JsonArray results = request.createNestedArray("p_results");
  const String encodedResults = message.substring(second + 1);
  int start = 0;
  while (start < encodedResults.length()) {
    const int end = encodedResults.indexOf(';', start);
    const String encoded = end < 0 ? encodedResults.substring(start) : encodedResults.substring(start, end);
    const int one = encoded.indexOf(',');
    const int two = encoded.indexOf(',', one + 1);
    if (one <= 0 || two <= one + 1) { sendError("BAD_RESULT_LINE"); return; }
    JsonObject result = results.add<JsonObject>();
    result["item_type"] = encoded.substring(0, one);
    result["product_id"] = encoded.substring(one + 1, two).toInt();
    result["qty_dispensed"] = encoded.substring(two + 1).toInt();
    if (end < 0) break;
    start = end + 1;
  }
  if (!callRpc("machine_finish_transaction", request, response)) return;
  MEGA_SERIAL.println("FINISHED:" + request["p_transaction_id"].as<String>() + ":" + response.as<String>());
}

void handleMegaMessage(String message) {
  message.trim();
  if (message.startsWith("RESERVE:")) reserveCart(message);
  else if (message.startsWith("CHANGE_OK:")) changePaid(message);
  else if (message.startsWith("CHANGE_FAIL:")) cancelReservation(message);
  else if (message.startsWith("FINISH:")) finishTransaction(message);
  else if (message == "STATUS?") sendWifiStatus();
}

// ================= SETUP / LOOP =================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.print("Reset reason: "); // PORTED FROM Cloud_remake5 - useful to
  Serial.println((int)esp_reset_reason()); // tell a crash-reboot apart from a clean power-on
  Serial.println("--- ESP32 CLOUD STARTING ---");

  MEGA_SERIAL.begin(9600, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  wifiConnected = connectToWifi(15000);
  sendWifiStatus();
  if (wifiConnected) {
    updateMachineStatus();
  }
}

void loop() {
  if (MEGA_SERIAL.available()) handleMegaMessage(MEGA_SERIAL.readStringUntil('\n'));

  ensureWifi();

  // WIFI: status ping to the Mega (unchanged from production_esp32)
  if (millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    sendWifiStatus();
  }

  // PORTED FROM Cloud_remake5: periodic Supabase health heartbeat
  if (wifiConnected && millis() - lastStatusUpdate > statusInterval) {
    updateMachineStatus();
    lastStatusUpdate = millis();
  }

  // PORTED FROM Cloud_remake5: non-blocking reconnect watchdog.
  // setAutoReconnect(true) handles routine drops on its own; this only
  // steps in with a full radio reset if the link has been down for a
  // sustained stretch.
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    bool nowConnected = WiFi.status() == WL_CONNECTED;

    if (nowConnected != wifiConnected) {
      wifiConnected = nowConnected;
      sendWifiStatus();
    }

    if (!nowConnected) {
      if (disconnectedSince == 0) {
        disconnectedSince = millis();
        Serial.println("WiFi disconnected. Waiting to see if auto-reconnect recovers it...");
      } else if (millis() - disconnectedSince > WIFI_STUCK_THRESHOLD) {
        Serial.println("WiFi still down after 20s - forcing full reconnect.");
        wifiConnected = connectToWifi(10000);
        sendWifiStatus();
        disconnectedSince = 0;
      }
    } else {
      disconnectedSince = 0;
    }
  }
}

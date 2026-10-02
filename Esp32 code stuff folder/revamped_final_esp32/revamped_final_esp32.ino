#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Esp.h>
#include <esp_system.h>
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
int maximumBallpensPerTransaction = 5;

// --- SUPABASE CONFIG ---
const char* SUPABASE_URL = "https://jowpzdynbdeznuvohrpx.supabase.co";
const char* SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

HardwareSerial &MEGA_SERIAL = Serial2;
const int MEGA_RX_PIN = 16;
const int MEGA_TX_PIN = 17;

bool wifiConnected = false;
unsigned long lastHeartbeatAt = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000; // Fallback status ping; Mega also polls STATUS?
unsigned long lastOnlineHeartbeatAt = 0;
const unsigned long ONLINE_HEARTBEAT_INTERVAL_MS = 10000; // Supabase heartbeat

// A finish request must survive a temporary Wi-Fi/API failure.  Keep the
// payload in RAM and retry it from loop() instead of leaving the Mega waiting.
bool pendingFinish = false;
String pendingFinishTransactionId;
String pendingFinishResults;
int pendingFinishChangePaidCents = 0;
bool pendingFinishChangeTimedOut = false;
unsigned long nextFinishRetryAt = 0;
uint8_t finishRetryCount = 0;
String lastRpcFailureReason = "";
int lastRpcFailureCode = 0;
String lastLoggedFinishFailureTransactionId = "";
const unsigned long FINISH_RETRY_INTERVAL_MS = 3000;
const uint8_t MAX_FINISH_RETRIES = 20;
#define FINISH_QUEUE_CAPACITY 8
struct QueuedFinish {
  String transactionId;
  String results;
  int changePaidCents;
  bool changeTimedOut;
};
QueuedFinish finishQueue[FINISH_QUEUE_CAPACITY];
uint8_t finishQueueCount = 0;
#define SYSTEM_EVENT_QUEUE_CAPACITY 16
struct QueuedSystemEvent {
  String level;
  String source;
  String eventType;
  String message;
  String rpcFunction;
  int httpCode;
};
QueuedSystemEvent systemEventQueue[SYSTEM_EVENT_QUEUE_CAPACITY];
uint8_t systemEventQueueCount = 0;
bool awaitingFinishAck = false;
String lastFinishedMessage;
String awaitingFinishTransactionId;
unsigned long lastFinishedSentAt = 0;
uint8_t finishAckAttempts = 0;
const unsigned long FINISH_ACK_RETRY_INTERVAL_MS = 500;
const uint8_t MAX_FINISH_ACK_ATTEMPTS = 10;
int pendingCreditSessionCents = -1;
unsigned long nextCreditSessionAttemptAt = 0;
const unsigned long CREDIT_SESSION_RETRY_INTERVAL_MS = 3000;
bool pendingCurrentCreditsStatus = false;
String pendingCurrentCreditsValue;
unsigned long nextCurrentCreditsStatusAttemptAt = 0;
const unsigned long CURRENT_CREDITS_STATUS_RETRY_INTERVAL_MS = 5000;
bool megaTransactionActive = false;
bool customerCreditSessionActive = false;
bool pendingCatalogSync = false;
bool pendingMachineStatusSync = false;
bool pendingRemoteNetworkConfigCheck = false;
bool pendingPaperBayEmpty[2] = { false, false };
unsigned long nextPaperBayUpdateAt = 0;
unsigned long nextSystemEventAttemptAt = 0;
unsigned long nextQueuedEventAttemptAt = 0;
unsigned long lastCustomerActivityAt = 0;
const unsigned long NETWORK_CONFIG_IDLE_GRACE_MS = 30000;
const unsigned long QUEUED_EVENT_RETRY_INTERVAL_MS = 3000;

#define HARDWARE_EVENT_QUEUE_CAPACITY 8
struct QueuedHardwareEvent {
  String component;
  String state;
};
QueuedHardwareEvent hardwareEventQueue[HARDWARE_EVENT_QUEUE_CAPACITY];
uint8_t hardwareEventQueueCount = 0;
unsigned long nextHardwareEventAttemptAt = 0;

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // machine_status table update

unsigned long lastWiFiCheck = 0;
unsigned long disconnectedSince = 0;
const unsigned long WIFI_CHECK_INTERVAL = 5000;
const unsigned long WIFI_STUCK_THRESHOLD = 20000;

bool connectToWifi(unsigned long timeoutMs);
bool printNearbyWifiNetworks();
void updateMachineStatus();
bool updateStatusKey(const String &key, const String &value);
bool callRpc(const char* functionName, JsonDocument &request,
             DynamicJsonDocument &response, unsigned long timeoutMs = 5000,
             bool reportErrorToMega = true);
bool persistCreditSession(int creditCents);
bool queueSystemEvent(const String &level, const String &source,
                      const String &eventType, const String &message,
                      const String &rpcFunction, int httpCode);
bool queueHardwareEvent(const String &component, const String &state);
void processPendingHardwareEvent();
void processPendingPaperBayUpdate();
void processPendingSystemEvent();
void processPendingCurrentCreditsStatus();
bool sendOnlineHeartbeat();
void softResetRuntime();
bool fetchAndApplyRemoteNetworkConfig();
bool acknowledgeRemoteNetworkConfig(const String &version);
bool submitFinishTransaction(const String &transactionId,
                             const String &encodedResults,
                             int changePaidCents,
                             bool changeTimedOut,
                             String &trNumber,
                             String &status,
                             int &dueCents,
                             int &paidCents);
void processPendingFinish();
bool enqueueFinish(const String &transactionId, const String &results,
                  int changePaidCents, bool changeTimedOut);
void promoteQueuedFinish();
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
  return WiFi.status() == WL_CONNECTED;
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
    // Report immediately instead of waiting for the caller or the next
    // heartbeat cycle. This keeps the Mega/TFT synchronized after Wi-Fi joins.
    sendWifiStatus();
    return true;
  } else {
    Serial.println("\nWiFi connection timed out. Continuing offline.");
    wifiConnected = false;
    sendWifiStatus();
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
  http.setConnectTimeout(1500);
  http.setTimeout(2500);
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
  http.setConnectTimeout(1500);
  http.setTimeout(2500);
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

bool updateStatusKey(const String &key, const String &value) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  // Upsert lets the gateway publish new runtime keys without a manual SQL seed.
  String url = String(SUPABASE_URL) + "/rest/v1/machine_status?on_conflict=status_key";

  if (http.begin(client, url)) {
    http.setConnectTimeout(1000);
    http.setTimeout(2000);
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
    return code >= 200 && code < 300;
  }
  return false;
}

bool persistCreditSession(int creditCents) {
  if (!ensureWifi()) return false;
  DynamicJsonDocument request(256), response(512);
  request["p_credit_cents"] = creditCents;
  if (!callRpc("machine_update_credit_session", request, response, 1500, false)) return false;
  pendingCreditSessionCents = -1;
  return true;
}

bool recordHardwareEvent(const String &component, const String &state) {
  if (!ensureWifi()) return false;
  DynamicJsonDocument request(384), response(256);
  request["p_component"] = component;
  request["p_state"] = state;
  request["p_message"] = component + (state == "DISCONNECTED"
    ? " is disconnected"
    : " reconnected");
  return callRpc("machine_record_hardware_event", request, response, 1500, false);
}

bool recordSystemEvent(const String &level, const String &source,
                       const String &eventType, const String &message,
                       const String &rpcFunction, int httpCode) {
  if (!ensureWifi()) return false;
  DynamicJsonDocument request(768), response(256);
  request["p_level"] = level;
  request["p_source"] = source;
  request["p_event_type"] = eventType;
  request["p_message"] = message;
  JsonObject metadata = request.createNestedObject("p_metadata");
  metadata["rpc_function"] = rpcFunction;
  metadata["http_code"] = httpCode;
  if (eventType == "MEGA_TRACE") metadata["trace_payload"] = message;
  return callRpc("machine_record_system_event", request, response, 1500, false);
}

bool isProtectedSystemEvent(const QueuedSystemEvent &event) {
  return event.eventType == "MEGA_TRACE" || event.level == "ERROR" ||
         event.eventType == "CHECKOUT_FAILED" ||
         event.eventType == "CHECKOUT_WATCHDOG_TIMEOUT" ||
         event.eventType == "ESP32_RESET" || event.eventType == "MEGA_RESET";
}

bool queueSystemEvent(const String &level, const String &source,
                      const String &eventType, const String &message,
                      const String &rpcFunction, int httpCode) {
  if (systemEventQueueCount >= SYSTEM_EVENT_QUEUE_CAPACITY) {
    int discardIndex = -1;
    for (uint8_t i = 0; i < systemEventQueueCount; i++) {
      if (!isProtectedSystemEvent(systemEventQueue[i])) {
        discardIndex = i;
        break;
      }
    }
    if (discardIndex < 0) {
      Serial.println("System-event queue full of critical events; diagnostic dropped.");
      return false;
    }
    for (uint8_t i = discardIndex + 1; i < systemEventQueueCount; i++)
      systemEventQueue[i - 1] = systemEventQueue[i];
    systemEventQueueCount--;
    Serial.println("System-event queue full; discarded noncritical event for diagnostic.");
  }
  QueuedSystemEvent &event = systemEventQueue[systemEventQueueCount++];
  event.level = level;
  event.source = source;
  event.eventType = eventType;
  event.message = message;
  event.rpcFunction = rpcFunction;
  event.httpCode = httpCode;
  return true;
}

bool queueHardwareEvent(const String &component, const String &state) {
  for (uint8_t i = 0; i < hardwareEventQueueCount; i++) {
    if (hardwareEventQueue[i].component == component &&
        hardwareEventQueue[i].state == state) return true;
  }
  if (hardwareEventQueueCount >= HARDWARE_EVENT_QUEUE_CAPACITY) {
    Serial.println("Hardware-event queue full; dropping hardware diagnostic.");
    return false;
  }
  QueuedHardwareEvent &event = hardwareEventQueue[hardwareEventQueueCount++];
  event.component = component;
  event.state = state;
  return true;
}

void processPendingHardwareEvent() {
  if (hardwareEventQueueCount == 0 || !ensureWifi() ||
      millis() < nextHardwareEventAttemptAt) return;
  QueuedHardwareEvent &event = hardwareEventQueue[0];
  if (!recordHardwareEvent(event.component, event.state)) {
    nextHardwareEventAttemptAt = millis() + 3000;
    return;
  }
  for (uint8_t i = 1; i < hardwareEventQueueCount; i++) {
    hardwareEventQueue[i - 1] = hardwareEventQueue[i];
  }
  hardwareEventQueueCount--;
  nextHardwareEventAttemptAt = 0;
}

void processPendingPaperBayUpdate() {
  if (millis() < nextPaperBayUpdateAt || !ensureWifi()) return;
  for (uint8_t i = 0; i < 2; i++) {
    if (!pendingPaperBayEmpty[i]) continue;

    const int bayNum = i + 1;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    const String url = String(SUPABASE_URL) +
      "/rest/v1/paper_compartments?compartment_number=eq." + String(bayNum);
    bool updated = false;
    if (http.begin(client, url)) {
      http.setConnectTimeout(1000);
      http.setTimeout(1500);
      http.addHeader("apikey", SUPABASE_ANON_KEY);
      http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
      http.addHeader("Content-Type", "application/json");
      const String body = "{\"presence_status\":\"LOW\",\"current_pad_stock\":0,\"updated_at\":\"now()\"}";
      const int code = http.PATCH(body);
      updated = code >= 200 && code < 300;
      http.end();
    }

    if (updated) {
      pendingPaperBayEmpty[i] = false;
      nextPaperBayUpdateAt = 0;
    } else {
      nextPaperBayUpdateAt = millis() + QUEUED_EVENT_RETRY_INTERVAL_MS;
    }
    return;
  }
}

void processPendingSystemEvent() {
  if (systemEventQueueCount == 0 || !ensureWifi() ||
      millis() < nextSystemEventAttemptAt) return;
  QueuedSystemEvent &event = systemEventQueue[0];
  if (!recordSystemEvent(event.level, event.source, event.eventType,
                         event.message, event.rpcFunction, event.httpCode)) {
    nextSystemEventAttemptAt = millis() + QUEUED_EVENT_RETRY_INTERVAL_MS;
    return;
  }
  Serial.println("Machine log persisted: " + event.eventType);
  for (uint8_t i = 1; i < systemEventQueueCount; i++) {
    systemEventQueue[i - 1] = systemEventQueue[i];
  }
  systemEventQueueCount--;
  nextSystemEventAttemptAt = 0;
}

void processPendingCreditSession() {
  if (pendingCreditSessionCents < 0 || millis() < nextCreditSessionAttemptAt) return;
  if (!persistCreditSession(pendingCreditSessionCents)) {
    nextCreditSessionAttemptAt = millis() + CREDIT_SESSION_RETRY_INTERVAL_MS;
  }
}

void processPendingCurrentCreditsStatus() {
  if (!pendingCurrentCreditsStatus || millis() < nextCurrentCreditsStatusAttemptAt) return;
  if (updateStatusKey("current_credits", pendingCurrentCreditsValue)) {
    pendingCurrentCreditsStatus = false;
    return;
  }
  nextCurrentCreditsStatusAttemptAt = millis() + CURRENT_CREDITS_STATUS_RETRY_INTERVAL_MS;
}

void handleCreditUpdate(String message) {
  int separator = message.indexOf(':');
  if (separator < 0) return;
  int credits = message.substring(separator + 1).toInt();
  if (credits < 0) return;
  customerCreditSessionActive = credits > 0;
  pendingCurrentCreditsValue = String(credits);
  lastCustomerActivityAt = millis();
  pendingCurrentCreditsStatus = true;
  nextCurrentCreditsStatusAttemptAt = 0;
  pendingCreditSessionCents = credits * 100;
  nextCreditSessionAttemptAt = 0;
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

  // The heartbeat is periodic and recoverable; cap its network wait so UART work resumes.
  http.setTimeout(1500);

  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  http.setConnectTimeout(1000);

  // The Supabase trigger updates last_heartbeat and updated_at with server time.
  const int code = http.PATCH("{\"status\":\"Online\"}");
  http.end();

  if (code < 200 || code >= 300) {
    Serial.printf("Online heartbeat failed: HTTP %d\n", code);
    return false;
  }
  return true;
}

bool callRpc(const char* functionName, JsonDocument &request,
             DynamicJsonDocument &response, unsigned long timeoutMs,
             bool reportErrorToMega) {
  lastRpcFailureReason = "";
  lastRpcFailureCode = 0;
  if (!ensureWifi()) {
    lastRpcFailureReason = "Wi-Fi is disconnected";
    lastRpcFailureCode = -1001;
    if (reportErrorToMega) sendError("WIFI_OFFLINE");
    if (String(functionName) == "machine_checkout_transaction_with_session") {
      queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                       "Wi-Fi is disconnected before checkout could be sent to the database",
                       functionName, lastRpcFailureCode);
    }
    return false;
  }
  String body;
  serializeJson(request, body);
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  const String url = String(SUPABASE_URL) + "/rest/v1/rpc/" + functionName;
  if (!http.begin(client, url)) {
    lastRpcFailureReason = "HTTPS request could not start";
    lastRpcFailureCode = -1000;
    // Tell the Mega immediately. Logging is best-effort and must never keep
    // the customer-facing transaction waiting for a checkout watchdog.
    if (reportErrorToMega) sendError("HTTPS_START_FAILED");
    if (String(functionName) == "machine_checkout_transaction_with_session") {
      queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                       "Checkout could not start HTTPS request",
                       functionName, -1000);
    }
    return false;
  }
  // Checkout/finish requests are on the customer's critical path. Fail
  // promptly and use the existing retry flow instead of blocking for a long
  // network timeout.
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
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
    lastRpcFailureReason = reason;
    lastRpcFailureCode = code;
    reason.replace(':', '-');
    reason.replace('\n', ' ');
    if (reason.length() > 90) reason = reason.substring(0, 90);
    const bool checkoutFailure = String(functionName) == "machine_checkout_transaction_with_session";
    if (checkoutFailure) {
      // Forward the failure before attempting the diagnostic log. A slow or
      // unavailable logging request must not look like a checkout hang.
      if (reportErrorToMega) {
        if (code <= 0) sendError("CHECKOUT_NETWORK_ERROR");
        else sendError("DATABASE_REJECTED:" + reason);
      }
      queueSystemEvent(
        "ERROR", "ESP32", "CHECKOUT_FAILED",
        "Checkout failed: " + reason,
        functionName, code);
    } else {
      if (reportErrorToMega) sendError("DATABASE_REJECTED:" + reason);
    }
    return false;
  }
  if (payload.length() == 0) {
    response.clear();
    return true;
  }
  if (deserializeJson(response, payload)) {
    lastRpcFailureReason = "Database returned invalid JSON";
    lastRpcFailureCode = code;
    if (String(functionName) == "machine_checkout_transaction_with_session") {
      // Send the usable error first; the diagnostic write can be slow.
      if (reportErrorToMega) sendError("DATABASE_RESPONSE_INVALID");
      queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                       "Checkout returned invalid database data",
                       functionName, code);
    } else {
      if (reportErrorToMega) sendError("DATABASE_RESPONSE_INVALID");
    }
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
    http.setConnectTimeout(1000);
    http.setTimeout(1200);
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
    http.setConnectTimeout(1000);
    http.setTimeout(1200);
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
  url = String(SUPABASE_URL) + "/rest/v1/machine_options?id=eq.1&select=minimum_credits,maximum_credits,minimum_ballpens_per_transaction,maximum_ballpens_per_transaction";
  if (http.begin(client, url)) {
    http.setConnectTimeout(1000);
    http.setTimeout(1200);
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    int code = http.GET();
    if (code == 200) {
      DynamicJsonDocument optionsDoc(512);
      if (deserializeJson(optionsDoc, http.getString()) == DeserializationError::Ok &&
          optionsDoc.as<JsonArray>().size() > 0) {
        JsonObject options = optionsDoc[0];
        maximumBallpensPerTransaction = constrain(options["maximum_ballpens_per_transaction"] | 5, 1, 5);
        MEGA_SERIAL.println("OPTIONS:" + String(options["minimum_credits"] | 1) + ":" +
                            String(options["maximum_credits"] | 30) + ":" +
                            String(options["minimum_ballpens_per_transaction"] | 1) + ":" +
                            String(maximumBallpensPerTransaction));
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

void checkoutCart(const String &message) {
  megaTransactionActive = true;
  lastCustomerActivityAt = millis();
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) {
    megaTransactionActive = false;
    sendError("BAD_CHECKOUT_FORMAT");
    queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                     "Checkout request has an invalid format",
                     "machine_checkout_transaction_with_session", 0);
    return;
  }
  const int creditCents = message.substring(first + 1, second).toInt();
  const String encodedLines = message.substring(second + 1);

  // This acknowledgement is deliberately sent before Wi-Fi/HTTPS work. It
  // proves that the ESP32 received the request and lets the Mega distinguish
  // a transport problem from a slow checkout request.
  MEGA_SERIAL.println("CHECKOUT_RECEIVED");
  Serial.println("Checkout request received from Mega.");
  DynamicJsonDocument request(2048);
  request["p_credit_cents"] = creditCents;
  JsonArray lines = request.createNestedArray("p_lines");
  int ballpenUnits = 0;
  int start = 0;
  while (start < encodedLines.length()) {
    const int end = encodedLines.indexOf(';', start);
    const String encoded = end < 0 ? encodedLines.substring(start) : encodedLines.substring(start, end);
    if (!parseCartLine(encoded, lines)) {
      megaTransactionActive = false;
      sendError("BAD_CART_LINE");
      queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                       "Checkout request contains an invalid product line",
                       "machine_checkout_transaction_with_session", 0);
      return;
    }
    const int comma1 = encoded.indexOf(',');
    const int comma2 = encoded.indexOf(',', comma1 + 1);
    if (encoded.substring(0, comma1) == "pen") {
      ballpenUnits += encoded.substring(comma2 + 1).toInt();
      if (ballpenUnits > maximumBallpensPerTransaction) {
        megaTransactionActive = false;
        sendError("MAX_BALLPENS");
        queueSystemEvent("WARNING", "ESP32", "CHECKOUT_REJECTED",
                         "Checkout rejected because ballpen quantity exceeds the configured limit",
                         "machine_checkout_transaction_with_session", 0);
        return;
      }
    }
    if (end < 0) break;
    start = end + 1;
  }
  DynamicJsonDocument response(4096);
  const unsigned long checkoutStartedAt = millis();
  // Keep checkout bounded by the single communication watchdog. Inventory is
  // validated at checkout but is not held in a reservation counter.
  if (!callRpc("machine_checkout_transaction_with_session", request, response, 5000)) {
    megaTransactionActive = false;
    return;
  }
  Serial.printf("Checkout completed in %lu ms.\n", millis() - checkoutStartedAt);
  JsonObject result = response[0];
  if (result.isNull()) {
    megaTransactionActive = false;
    sendError("EMPTY_CHECKOUT");
    queueSystemEvent("ERROR", "ESP32", "CHECKOUT_FAILED",
                     "Checkout returned no transaction",
                     "machine_checkout_transaction_with_session", 200);
    return;
  }

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

bool submitFinishTransaction(const String &transactionId,
                             const String &encodedResults,
                             int changePaidCents,
                             bool changeTimedOut,
                             String &trNumber,
                             String &status,
                             int &dueCents,
                             int &paidCents) {
  DynamicJsonDocument request(2048), response(1024);
  request["p_transaction_id"] = transactionId;
  request["p_change_paid_cents"] = changePaidCents;
  request["p_change_release_timed_out"] = changeTimedOut;
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

  if (!callRpc("machine_finish_transaction", request, response, 5000, false)) return false;

  JsonObject res = response[0];
  if (res.isNull()) {
    lastRpcFailureReason = "Finish RPC returned no transaction result";
    lastRpcFailureCode = 200;
    return false;
  }
  trNumber = res["tr_number"] | "TR-00000";
  status = res["final_status"] | "COMPLETED";
  dueCents = res["change_due_cents"] | 0;
  paidCents = res["change_paid_cents"] | 0;
  return true;
}

bool enqueueFinish(const String &transactionId, const String &results,
                   int changePaidCents, bool changeTimedOut) {
  if (finishQueueCount >= FINISH_QUEUE_CAPACITY) return false;
  QueuedFinish &job = finishQueue[finishQueueCount++];
  job.transactionId = transactionId;
  job.results = results;
  job.changePaidCents = changePaidCents;
  job.changeTimedOut = changeTimedOut;
  return true;
}

void promoteQueuedFinish() {
  if (pendingFinish || finishQueueCount == 0) return;
  pendingFinishTransactionId = finishQueue[0].transactionId;
  pendingFinishResults = finishQueue[0].results;
  pendingFinishChangePaidCents = finishQueue[0].changePaidCents;
  pendingFinishChangeTimedOut = finishQueue[0].changeTimedOut;
  for (uint8_t i = 1; i < finishQueueCount; i++) {
    finishQueue[i - 1] = finishQueue[i];
  }
  finishQueueCount--;
  pendingFinish = true;
  finishRetryCount = 0;
  nextFinishRetryAt = 0;
}

void logPendingFinishFailureOnce() {
  if (lastLoggedFinishFailureTransactionId == pendingFinishTransactionId) return;
  String reason = lastRpcFailureReason.length()
    ? lastRpcFailureReason
    : "Finish RPC response could not be parsed";
  reason.replace(':', '-');
  reason.replace('\n', ' ');
  queueSystemEvent("ERROR", "ESP32", "TRANSACTION_FINALIZATION_FAILED",
                   "Could not finalize transaction " + pendingFinishTransactionId +
                   "; retrying database save: " + reason,
                   "machine_finish_transaction", lastRpcFailureCode);
  lastLoggedFinishFailureTransactionId = pendingFinishTransactionId;
}

void processPendingFinish() {
  if (awaitingFinishAck) {
    if (millis() - lastFinishedSentAt < FINISH_ACK_RETRY_INTERVAL_MS) return;
    if (finishAckAttempts >= MAX_FINISH_ACK_ATTEMPTS) {
      queueSystemEvent("ERROR", "ESP32", "FINISHED_DELIVERY_UNCONFIRMED",
                       "Transaction finished in the database but Mega did not acknowledge the TFT completion message",
                       "machine_finish_transaction", 0);
      awaitingFinishAck = false;
      awaitingFinishTransactionId = "";
      return;
    }
    MEGA_SERIAL.println(lastFinishedMessage);
    lastFinishedSentAt = millis();
    finishAckAttempts++;
    return;
  }

  promoteQueuedFinish();
  if (!pendingFinish || millis() < nextFinishRetryAt) return;

  finishRetryCount++;
  if (!ensureWifi()) {
    lastRpcFailureReason = "Wi-Fi is disconnected";
    lastRpcFailureCode = -1001;
    logPendingFinishFailureOnce();
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
                              pendingFinishChangeTimedOut,
                              trNumber,
                              status,
                              dueCents,
                              paidCents)) {
    lastFinishedMessage = "FINISHED:" + pendingFinishTransactionId + ":" +
                          trNumber + ":" + status + ":" +
                          String(dueCents) + ":" + String(paidCents);
    awaitingFinishTransactionId = pendingFinishTransactionId;
    lastLoggedFinishFailureTransactionId = "";
    MEGA_SERIAL.println(lastFinishedMessage);
    lastFinishedSentAt = millis();
    finishAckAttempts = 1;
    awaitingFinishAck = true;
    pendingFinish = false;
    finishRetryCount = 0;
    nextFinishRetryAt = 0;
    return;
  }

  logPendingFinishFailureOnce();

  if (finishRetryCount == 1) sendError("FINISH_PENDING");
  if (finishRetryCount >= MAX_FINISH_RETRIES) {
    sendError("FINISH_RETRY_FAILED");
    queueSystemEvent("ERROR", "ESP32", "TRANSACTION_FINALIZATION_RETRYING",
                     "Transaction completion is still pending after repeated database attempts",
                     "machine_finish_transaction", 0);
    Serial.println("Finish remains queued; retrying after a longer backoff.");
    finishRetryCount = 0;
    nextFinishRetryAt = millis() + 15000;
    return;
  }

  nextFinishRetryAt = millis() + FINISH_RETRY_INTERVAL_MS;
}

void finishTransaction(const String &message) {
  // Format from Mega: FINISH:<tx_id>:<encodedResults>:<change_paid_cents>:<change_timeout>
  const int first = message.indexOf(':');
  const int second = message.indexOf(':', first + 1);
  if (second < 0) { sendError("BAD_FINISH_FORMAT"); return; }
  const String transactionId = message.substring(first + 1, second);

  int third = message.indexOf(':', second + 1);
  int fourth = third < 0 ? -1 : message.indexOf(':', third + 1);
  String encodedResults;
  int changePaidCents = 0;
  bool changeTimedOut = false;
  if (third > 0) {
    encodedResults = message.substring(second + 1, third);
    if (fourth > 0) {
      changePaidCents = message.substring(third + 1, fourth).toInt();
      changeTimedOut = message.substring(fourth + 1).toInt() == 1;
    } else {
      changePaidCents = message.substring(third + 1).toInt();
    }
  } else {
    encodedResults = message.substring(second + 1);
  }

  if (pendingFinish) {
    if (!enqueueFinish(transactionId, encodedResults, changePaidCents, changeTimedOut)) {
      sendError("FINISH_QUEUE_FULL");
      queueSystemEvent("ERROR", "ESP32", "FINISH_QUEUE_FULL",
                       "Could not queue transaction completion; finish queue is full",
                       "machine_finish_transaction", 0);
      return;
    }
    Serial.printf("Queued transaction completion. Queue depth: %u\n", finishQueueCount);
  } else {
    pendingFinish = true;
    pendingFinishTransactionId = transactionId;
    pendingFinishResults = encodedResults;
    pendingFinishChangePaidCents = changePaidCents;
    pendingFinishChangeTimedOut = changeTimedOut;
    finishRetryCount = 0;
    nextFinishRetryAt = 0;
  }
  processPendingFinish();
}

void handleMegaMessage(String message) {
  message.trim();
  if (message.startsWith("CREDIT:")) handleCreditUpdate(message);
  else if (message.startsWith("CHECKOUT:")) checkoutCart(message);
  else if (message.startsWith("CHANGE_OK:")) changePaid(message);
  else if (message.startsWith("FINISHED_ACK:")) {
    const String transactionId = message.substring(13);
    if (awaitingFinishAck && transactionId == awaitingFinishTransactionId) {
      awaitingFinishAck = false;
      megaTransactionActive = false;
      lastFinishedMessage = "";
      awaitingFinishTransactionId = "";
      finishAckAttempts = 0;
    }
  }
  else if (message.startsWith("FINISH:")) finishTransaction(message);
  else if (message.startsWith("BAY_EMPTY:")) {
    const int bayNum = message.substring(10).toInt();
    if (bayNum >= 1 && bayNum <= 2) pendingPaperBayEmpty[bayNum - 1] = true;
  }
  else if (message.startsWith("STAGE_ERROR:")) {
    const int first = message.indexOf(':');
    const int second = message.indexOf(':', first + 1);
    if (second > first) {
      const String stage = message.substring(first + 1, second);
      const String reason = message.substring(second + 1);
      const bool checkoutTimeout = stage == "CHECKOUT";
      if (checkoutTimeout) megaTransactionActive = false;
      queueSystemEvent(
        "ERROR", "MEGA",
        checkoutTimeout ? "CHECKOUT_WATCHDOG_TIMEOUT" : "TRANSACTION_STAGE_TIMEOUT",
        checkoutTimeout
          ? "Checkout unavailable: Mega watchdog expired (" + reason + ")"
          : "Transaction stage " + stage + " failed: " + reason,
        checkoutTimeout ? "machine_checkout_transaction_with_session" : "", 0);
    }
  }
  else if (message.startsWith("DBG:")) {
    const String traceMessage = message.substring(4);
    Serial.println("Mega trace received: " + traceMessage);
    if (!queueSystemEvent("INFO", "MEGA", "MEGA_TRACE", traceMessage, "", 0))
      Serial.println("Mega trace could not be queued for database logging.");
  }
  else if (message.startsWith("HARDWARE_EVENT:")) {
    const int first = message.indexOf(':');
    const int second = message.indexOf(':', first + 1);
    if (second > first) {
      const String component = message.substring(first + 1, second);
      const String state = message.substring(second + 1);
      if (!queueHardwareEvent(component, state))
        Serial.println("Hardware event could not be queued: " + component + " " + state);
    }
  }
  else if (message.startsWith("MEGA_RESET_CAUSE:")) {
    megaTransactionActive = false;
    const String cause = message.substring(17);
    queueSystemEvent(
      "ERROR", "MEGA", "MEGA_RESET",
      "Mega restarted; reset cause code " + cause,
      "", 0);
  }
  else if (message == "GET_CATALOG") pendingCatalogSync = true;
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
  lastOnlineHeartbeatAt = 0;
  lastStatusUpdate = 0;
  lastWiFiCheck = 0;
  disconnectedSince = 0;
  // Tell the Mega/TFT immediately; the reconnect attempt can take several seconds.
  MEGA_SERIAL.println("WIFISTATE:CONNECTING");
  sendWifiStatus();
  wifiConnected = connectToWifi(10000);
  sendWifiStatus();
  if (wifiConnected) {
    lastOnlineHeartbeatAt = 0;
    pendingMachineStatusSync = true;
    pendingCatalogSync = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- REVAMPED ESP32 CLOUD GATEWAY STARTING ---");

  const esp_reset_reason_t resetReason = esp_reset_reason();
  if (resetReason != ESP_RST_POWERON) {
    Serial.printf("ESP32 reset reason code: %d\n", (int)resetReason);
  }

  MEGA_SERIAL.begin(9600, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);
  MEGA_SERIAL.setTimeout(200);

  loadSavedWifiCredentials();
  wifiConnected = connectUsingSavedFallbacks();
  sendWifiStatus();
  if (wifiConnected) {
    if (resetReason != ESP_RST_POWERON) {
      queueSystemEvent(
        "ERROR", "ESP32", "ESP32_RESET",
        "ESP32 restarted; reset reason code " + String((int)resetReason),
        "", 0);
    }
    pendingRemoteNetworkConfigCheck = true;
    pendingMachineStatusSync = true;
    pendingCatalogSync = true;
  }
}

void loop() {
  // Drain UART before doing anything that can wait on Wi-Fi or HTTPS.
  uint8_t messagesRead = 0;
  while (MEGA_SERIAL.available() && messagesRead < 24) {
    String message = MEGA_SERIAL.readStringUntil('\n');
    if (message.length()) handleMegaMessage(message);
    messagesRead++;
  }
  const bool megaHasQueuedMessages = MEGA_SERIAL.available() > 0;

  // Detect connectivity edges here. ensureWifi() only queries the radio, so
  // request handlers cannot overwrite this transition before it is reported.
  const bool connectedNow = WiFi.status() == WL_CONNECTED;
  if (connectedNow != wifiConnected) {
    wifiConnected = connectedNow;
    sendWifiStatus();
    if (wifiConnected) {
      disconnectedSince = 0;
      pendingRemoteNetworkConfigCheck = true;
    } else if (disconnectedSince == 0) {
      disconnectedSince = millis();
    }
  }

  // Always service the Mega status request before beginning another HTTP call.
  if (millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    sendWifiStatus();
  }

  // Run only one queued credit write per loop pass. The session RPC is the
  // durable record; current_credits is a best-effort monitor display value.
  if (!megaTransactionActive && !megaHasQueuedMessages && pendingCreditSessionCents >= 0 &&
      millis() >= nextCreditSessionAttemptAt) {
    processPendingCreditSession();
  } else if (!megaTransactionActive && !customerCreditSessionActive &&
             !megaHasQueuedMessages) {
    processPendingCurrentCreditsStatus();
  }
  processPendingFinish();

  const bool customerSessionIdle = !megaTransactionActive && !customerCreditSessionActive;
  if (wifiConnected && !megaHasQueuedMessages &&
      millis() - lastOnlineHeartbeatAt >= ONLINE_HEARTBEAT_INTERVAL_MS) {
    lastOnlineHeartbeatAt = millis();
    sendOnlineHeartbeat();
  }

  if (wifiConnected && customerSessionIdle && !megaHasQueuedMessages &&
      millis() - lastStatusUpdate > statusInterval) {
    pendingMachineStatusSync = true;
    pendingCatalogSync = true;
    lastStatusUpdate = millis();
  }

  // Run at most one noncritical network operation during idle time. UART is
  // drained first on the next pass, keeping checkout ahead of diagnostics.
  if (!megaTransactionActive && !megaHasQueuedMessages &&
      millis() >= nextQueuedEventAttemptAt) {
    if (hardwareEventQueueCount > 0 && millis() >= nextHardwareEventAttemptAt)
      processPendingHardwareEvent();
    else if (systemEventQueueCount > 0 && millis() >= nextSystemEventAttemptAt)
      processPendingSystemEvent();
    else if (customerSessionIdle &&
             (pendingPaperBayEmpty[0] || pendingPaperBayEmpty[1]) &&
             millis() >= nextPaperBayUpdateAt)
      processPendingPaperBayUpdate();
    else if (customerSessionIdle && pendingMachineStatusSync && wifiConnected) {
      updateMachineStatus();
      pendingMachineStatusSync = false;
    }
    else if (customerSessionIdle && pendingCatalogSync && wifiConnected) {
      syncLiveCatalogToMega();
      pendingCatalogSync = false;
    } else if (customerSessionIdle && pendingRemoteNetworkConfigCheck && wifiConnected &&
               (lastCustomerActivityAt == 0 ||
                millis() - lastCustomerActivityAt >= NETWORK_CONFIG_IDLE_GRACE_MS)) {
      pendingRemoteNetworkConfigCheck = false;
      fetchAndApplyRemoteNetworkConfig();
    }
    nextQueuedEventAttemptAt = millis() + 25;
  }

  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    bool nowConnected = WiFi.status() == WL_CONNECTED;

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

  if (wifiConnected && customerSessionIdle && !megaHasQueuedMessages &&
      millis() - lastNetworkConfigCheck >= NETWORK_CONFIG_CHECK_INTERVAL) {
    lastNetworkConfigCheck = millis();
    pendingRemoteNetworkConfigCheck = true;
  }
}

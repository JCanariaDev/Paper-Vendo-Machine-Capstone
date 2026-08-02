#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/*
  Production ESP32 gateway
  Serial protocol with Mega:
    RESERVE:<creditCents>:paper,productId,units;pen,productId,units
    CHANGE_OK:<transactionId>:<changeCents>
    CHANGE_FAIL:<transactionId>:<reason>
    FINISH:<transactionId>:paper,productId,qty;pen,productId,qty
    STATUS?

  Responses:
    RESERVED:<transactionId>:<subtotalCents>:<changeCents>
    PLAN:<transactionId>:paper,productId,channel,qty;pen,productId,channel,qty
    WIFI:0|1
    ERR:<safe message>

  Configure the Wi-Fi and Supabase values before upload. Keep the current
  Cloud_Edition sketch unchanged until this production flow is hardware-tested.
*/

const char* WIFI_SSID = "REPLACE_WITH_WIFI_SSID";
const char* WIFI_PASSWORD = "REPLACE_WITH_WIFI_PASSWORD";
const char* SUPABASE_URL = "https://REPLACE_WITH_PROJECT.supabase.co";
const char* SUPABASE_ANON_KEY = "REPLACE_WITH_ANON_KEY";

HardwareSerial &MEGA_SERIAL = Serial2;
const int MEGA_RX_PIN = 16;
const int MEGA_TX_PIN = 17;

bool wifiConnected = false;
unsigned long lastHeartbeatAt = 0;
const unsigned long HEARTBEAT_INTERVAL_MS = 5000;

void sendError(const String &message) {
  MEGA_SERIAL.println("ERR:" + message);
}

void sendWifiStatus() {
  MEGA_SERIAL.println("WIFI:" + String(wifiConnected ? 1 : 0));
}

bool ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    return true;
  }
  wifiConnected = false;
  return false;
}

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

void setup() {
  Serial.begin(115200);
  MEGA_SERIAL.begin(9600, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  if (MEGA_SERIAL.available()) handleMegaMessage(MEGA_SERIAL.readStringUntil('\n'));
  ensureWifi();
  if (millis() - lastHeartbeatAt >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatAt = millis();
    sendWifiStatus();
  }
}

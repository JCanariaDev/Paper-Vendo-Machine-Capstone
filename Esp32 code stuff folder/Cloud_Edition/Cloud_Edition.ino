#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/*
  Cloud_remake3.ino - Gateway for Paper Vendo Machine
  REVISION 3: Touchscreen moved to the Mega (see mega_1.ino). This sketch
  is now a pure network gateway - TFT_UI.ino is no longer needed and
  should be removed from this sketch folder.
*/

// --- WIFI CONFIG ---
const char* ssid = "ashid";
const char* password = "paltankolang";

// --- SUPABASE CONFIG ---
const String supabase_url = "https://jowpzdynbdeznuvohrpx.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // Update status every 60 seconds
int currentCredits = 0;
bool currentWifiConnected = false;

bool connectToWifi(unsigned long timeoutMs);
void printNearbyWifiNetworks();
void submitTftOrder(String type, int id, int qty);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.print("Reset reason: ");
  Serial.println((int)esp_reset_reason());
  // 0=UNKNOWN 1=POWERON 2=EXT 3=SW 4=PANIC 5=INT_WDT 6=TASK_WDT 7=WDT 8=DEEPSLEEP 9=BROWNOUT 10=SDIO
  Serial.println("--- ESP32 CLOUD STARTING ---");

  // Serial2 for Mega (RX=16, TX=17)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  currentWifiConnected = connectToWifi(15000);
  Serial2.println("WIFI:" + String(currentWifiConnected ? 1 : 0));

  if (currentWifiConnected) {
    updateMachineStatus(); // Update status on startup
  }
}

bool connectToWifi(unsigned long timeoutMs) {
  static bool wifiEverStarted = false;
  Serial.println("Initializing WiFi...");

  if (wifiEverStarted) {
    // Only tear the driver down if it was actually running before.
    // Calling WiFi.mode(WIFI_OFF) + disconnect(true, true) [eraseAP=true]
    // against a driver that has never been initialized (i.e. on the very
    // first boot) hits an invalid internal FreeRTOS event-group state and
    // panics with "assert failed: xEventGroupSetBits". Skipping this on
    // first boot avoids that; it still runs on genuine reconnects.
    WiFi.disconnect(true, false);
    delay(300);
  }
  wifiEverStarted = true;

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
  printNearbyWifiNetworks();

  Serial.println("Starting WiFi connection...");
  WiFi.begin(ssid, password);
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
    return true;
  } else {
    Serial.println("\nWiFi connection timed out. Continuing offline.");
    Serial.print("WiFi status code: ");
    Serial.println((int)WiFi.status());
    Serial.println("Mega serial communication remains available.");
    return false;
  }
}

void printNearbyWifiNetworks() {
  Serial.println("Scanning WiFi networks...");
  int networkCount = WiFi.scanNetworks();

  if (networkCount <= 0) {
    Serial.println("No WiFi networks found.");
    return;
  }

  bool targetFound = false;
  Serial.print("Networks found: ");
  Serial.println(networkCount);

  for (int i = 0; i < networkCount; i++) {
    String foundSsid = WiFi.SSID(i);
    if (foundSsid == ssid) {
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
}

void loop() {
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();

    if (incoming.startsWith("CREDIT:")) {
      currentCredits = incoming.substring(7).toInt();
    }
    else if (incoming.startsWith("REQ:")) {
      handleRequest(incoming);
    }
    else if (incoming.startsWith("TFTORDER:")) {
      handleTftOrder(incoming);
    }
    else if (incoming.startsWith("DONE:")) {
      handleLog(incoming);
    }
    // Note: "ERR:" is something *this* sketch sends TO the Mega now,
    // not something it receives - no handler needed for it here.
  }

  // Periodic Status Update
  if (millis() - lastStatusUpdate > statusInterval) {
    updateMachineStatus();
    lastStatusUpdate = millis();
  }

  // --- Non-blocking WiFi Reconnection Check ---
  // WiFi.setAutoReconnect(true) already handles routine reconnects on its
  // own. We only step in with a full radio reset (connectToWifi) if the
  // link has been down for a sustained stretch.
  static unsigned long lastWiFiCheck = 0;
  static unsigned long disconnectedSince = 0;
  const unsigned long WIFI_CHECK_INTERVAL = 5000;   // poll status every 5s
  const unsigned long WIFI_STUCK_THRESHOLD = 20000; // only hard-reset after 20s down

  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    bool wifiConnected = WiFi.status() == WL_CONNECTED;

    if (wifiConnected != currentWifiConnected) {
      currentWifiConnected = wifiConnected;
      Serial2.println("WIFI:" + String(currentWifiConnected ? 1 : 0));
    }

    if (!wifiConnected) {
      if (disconnectedSince == 0) {
        disconnectedSince = millis();
        Serial.println("WiFi disconnected. Waiting to see if auto-reconnect recovers it...");
      } else if (millis() - disconnectedSince > WIFI_STUCK_THRESHOLD) {
        Serial.println("WiFi still down after 20s - forcing full reconnect.");
        currentWifiConnected = connectToWifi(10000);
        Serial2.println("WIFI:" + String(currentWifiConnected ? 1 : 0));
        disconnectedSince = 0;
      }
    } else {
      disconnectedSince = 0;
    }
  }
}

void handleRequest(String msg) {
  // Format: REQ:TYPE:ID:COINS
  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  int third = msg.indexOf(':', second + 1);

  String type = msg.substring(first + 1, second);
  String id = msg.substring(second + 1, third);
  float coins = msg.substring(third + 1).toFloat();

  fetchAndValidate(type, id, coins);
}

void fetchAndValidate(String type, String id, float coins) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
  String cols = (type == "paper") ? "cost_per_unit,sheets_per_unit,paper_size" : "cost_per_unit,item_name";
  String url = supabase_url + "/rest/v1/" + table + "?id=eq." + id + "&select=" + cols;

  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    if (doc.size() > 0) {
      float cost = doc[0]["cost_per_unit"];
      String name = (type == "paper") ? doc[0]["paper_size"].as<String>() : doc[0]["item_name"].as<String>();

      if (coins >= cost) {
        if (type == "paper") {
          // --- BULK PAPER LOGIC ---
          int units = (int)(coins / cost);
          int sheetsPerUnit = doc[0]["sheets_per_unit"];
          int totalSheets = units * sheetsPerUnit;
          float totalCost = units * cost;

          Serial2.print("DISPENSE:");
          Serial2.print(totalSheets); Serial2.print(":");
          Serial2.print(totalCost); Serial2.print(":");
          Serial2.println(name);
        } else {
          // --- PEN LOGIC (1 unit + Change) ---
          Serial2.print("DISPENSE:1:");
          Serial2.print(cost); Serial2.print(":");
          Serial2.println(name);
        }
      } else {
        Serial2.println("ERR:LOW_CREDIT");
      }
    } else {
      Serial2.println("ERR:NOT_FOUND");
    }
  } else {
    Serial2.println("ERR:CLOUD_ERROR");
  }
  http.end();
}

void handleTftOrder(String msg) {
  // Format: TFTORDER:TYPE:ID:QTY  (sent by the Mega's touchscreen UI)
  int f1 = msg.indexOf(':');
  int f2 = msg.indexOf(':', f1 + 1);
  int f3 = msg.indexOf(':', f2 + 1);

  String type = msg.substring(f1 + 1, f2);
  int id = msg.substring(f2 + 1, f3).toInt();
  int qty = msg.substring(f3 + 1).toInt();

  submitTftOrder(type, id, qty);
}

void submitTftOrder(String type, int id, int qty) {
  if (qty <= 0) {
    Serial2.println("ERR:No quantity selected");
    return;
  }

  fetchAndValidateTftOrder(type, String(id), qty, currentCredits);
}

void fetchAndValidateTftOrder(String type, String id, int qty, float coins) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
  String cols = (type == "paper") ? "cost_per_unit,sheets_per_unit,paper_size" : "cost_per_unit,item_name";
  String url = supabase_url + "/rest/v1/" + table + "?id=eq." + id + "&select=" + cols;

  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    if (doc.size() > 0) {
      float unitCost = doc[0]["cost_per_unit"];
      float totalCost = unitCost * qty;
      String name = (type == "paper") ? doc[0]["paper_size"].as<String>() : doc[0]["item_name"].as<String>();
      int dispenseQty = qty;

      if (type == "paper") {
        int sheetsPerUnit = doc[0]["sheets_per_unit"];
        dispenseQty = qty * sheetsPerUnit;
      }

      if (coins >= totalCost) {
        Serial2.print("DISPENSE:");
        Serial2.print(type); Serial2.print(":");
        Serial2.print(id); Serial2.print(":");
        Serial2.print(dispenseQty); Serial2.print(":");
        Serial2.print(totalCost); Serial2.print(":");
        Serial2.println(name);
        Serial.println("TFT order sent to Mega: " + type + " " + id + " x" + String(qty));
      } else {
        Serial2.println("ERR:LOW_CREDIT");
      }
    } else {
      Serial2.println("ERR:NOT_FOUND");
    }
  } else {
    Serial2.println("ERR:CLOUD_ERROR");
  }

  http.end();
}

void handleLog(String msg) {
  // Format: DONE:TYPE:ID:NAME:PRICE:QTY
  int t1 = msg.indexOf(':') + 1;
  int t2 = msg.indexOf(':', t1);
  int t3 = msg.indexOf(':', t2 + 1);
  int t4 = msg.indexOf(':', t3 + 1);
  int t5 = msg.indexOf(':', t4 + 1);

  String type = msg.substring(t1, t2);
  String id = msg.substring(t2 + 1, t3);
  String name = msg.substring(t3 + 1, t4);
  String price = msg.substring(t4 + 1, t5);
  String qty = msg.substring(t5 + 1);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = supabase_url + "/rest/v1/sales_transactions";

  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"item_type\":\"" + type + "\", \"brand_id\":" + id + ", \"paper_size\":\"" + name + "\", \"amount_paid\":" + price + ", \"qty_dispensed\":" + qty + "}";
  http.POST(body);
  http.end();
  Serial.println(">>> TRANSACTION LOGGED.");
}

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

void updateStatusKey(String key, String value) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = supabase_url + "/rest/v1/machine_status?status_key=eq." + key;

  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");

  String body = "{\"status_value\":\"" + value + "\", \"updated_at\":\"now()\"}";
  int httpCode = http.PATCH(body);
  http.end();
}

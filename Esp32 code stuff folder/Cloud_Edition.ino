#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/*
  Cloud_Edition.ino - Final Production Version
  Gateway for Paper Vendo Machine
*/

// --- WIFI CONFIG ---
const char* ssid = "realme C3";
//const char* ssid = "Converge_X76J";
const char* password = "lancelot";
//const char* password = "Patokjeep02";

//const char* ssid = "ashid";
//const char* password = "paltankolang";

// --- SUPABASE CONFIG ---
const String supabase_url = "https://jowpzdynbdeznuvohrpx.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // Update status every 60 seconds

void setup() {
  Serial.begin(115200);   
  // Serial2 for Mega (RX=16, TX=17)
  Serial2.begin(9600, SERIAL_8N1, 16, 17); 
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! Machine Online.");
  updateMachineStatus(); // Update status on startup
}

void loop() {
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();
    
    if (incoming.startsWith("REQ:")) {
      handleRequest(incoming);
    } 
    else if (incoming.startsWith("DONE:")) {
      handleLog(incoming);
    }
  }

  // Periodic Status Update
  if (millis() - lastStatusUpdate > statusInterval) {
    updateMachineStatus();
    lastStatusUpdate = millis();
  }

  // Non-blocking WiFi Reconnection Check (every 10 seconds)
  static unsigned long lastWiFiCheck = 0;
  if (millis() - lastWiFiCheck > 10000) {
    lastWiFiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
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

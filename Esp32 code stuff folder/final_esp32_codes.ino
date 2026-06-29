#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/*
  final_esp32_codes.ino - Complete Production Version
  Gateway for Paper & Pen Vendo Machine (OLED & Dynamic Stock Management Version)
  REMOVED: Old REQ flow
  ADDED: GET_INFO handler, SET_STOCK handler, preserved DONE transaction logging
*/

// --- WIFI CONFIG (Preserved from your original code) ---
const char* ssid = "realme C3";
//const char* ssid = "Converge_X76J";
const char* password = "lancelot";
//const char* password = "Patokjeep02";

//const char* ssid = "ashid";
//const char* password = "paltankolang";

// --- SUPABASE CONFIG (Preserved from your original code) ---
const String supabase_url = "https://jowpzdynbdeznuvohrpx.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

unsigned long lastStatusUpdate = 0;
const unsigned long statusInterval = 60000; // Update status every 60 seconds

void setup() {
  Serial.begin(115200);   
  // Serial2 for Mega communication (RX=16, TX=17)
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
    
    if (incoming.startsWith("GET_INFO:")) {
      handleGetInfo(incoming);
    } 
    else if (incoming.startsWith("SET_STOCK:")) {
      handleSetStock(incoming);
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

// Handler for GET_INFO:type:id
// Fetches the cost, sheets per unit, current stock, and name of an item
void handleGetInfo(String msg) {
  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  
  String type = msg.substring(first + 1, second);
  String id = msg.substring(second + 1);
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
  String cols = (type == "paper") ? "cost_per_unit,sheets_per_unit,current_stock,brand_name" : "cost_per_unit,current_stock,item_name";
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
      int stock = doc[0]["current_stock"];
      int sheets = (type == "paper") ? doc[0]["sheets_per_unit"].as<int>() : 1;
      String name = (type == "paper") ? doc[0]["brand_name"].as<String>() : doc[0]["item_name"].as<String>();
      
      // Return details: INFO:cost:sheetsPerUnit:stock:name
      Serial2.print("INFO:");
      Serial2.print(cost); Serial2.print(":");
      Serial2.print(sheets); Serial2.print(":");
      Serial2.print(stock); Serial2.print(":");
      Serial2.println(name);
      
      Serial.println("Sent INFO back to Mega for ID " + id);
    } else {
      Serial2.println("INFO_ERR:NOT_FOUND");
    }
  } else {
    Serial2.println("INFO_ERR:CLOUD_ERROR");
  }
  http.end();
}

// Handler for SET_STOCK:type:id:qty
// Updates the stock of an item in Supabase via PATCH request
void handleSetStock(String msg) {
  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  int third = msg.indexOf(':', second + 1);
  
  String type = msg.substring(first + 1, second);
  String id = msg.substring(second + 1, third);
  int newQty = msg.substring(third + 1).toInt();
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
  String url = supabase_url + "/rest/v1/" + table + "?id=eq." + id;
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  
  String body = "{\"current_stock\":" + String(newQty) + "}";
  int httpCode = http.PATCH(body);
  
  // Supabase replies with HTTP 204 (No Content) or HTTP 200 on successful PATCH
  if (httpCode == 200 || httpCode == 204) {
    Serial2.println("SET_STOCK:OK");
    Serial.println("Admin updated stock on Cloud for " + type + " ID: " + id + " to: " + String(newQty));
  } else {
    Serial2.println("SET_STOCK:ERR");
    Serial.println("Error updating stock. HTTP Code: " + String(httpCode));
  }
  http.end();
}

// Handler for DONE:type:id:name:price:qty
// Log a sale transaction to the database
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
  
  // Map standard type names: pen translates to pen, paper to paper.
  // Note: the backend SQL trigger automatically decrements stock based on sales_transactions.
  String body = "{\"item_type\":\"" + type + "\", \"brand_id\":" + id + ", \"paper_size\":\"" + name + "\", \"amount_paid\":" + price + ", \"qty_dispensed\":" + qty + "}";
  int httpCode = http.POST(body);
  http.end();
  
  Serial.println(">>> TRANSACTION LOGGED. HTTP Code: " + String(httpCode));
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

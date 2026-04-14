#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

/*
  MockUp_ESP32.ino
  Connect Arduino Pin 1 (TX) to ESP32 Pin 16 (RX2).
  IMPORTANT: Both the Arduino and ESP32 MUST share a common GROUND (GND) pin.
*/

// --- WIFI CONFIG ---
// const char* ssid = "realme C3";
// const char* password = "lancelot";

const char* ssid = "HericoSnap 2.4G";
const char* password = "HericoAA24";

// --- SUPABASE CONFIG ---
const String supabase_url = "https://jowpzdynbdeznuvohrpx.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

void setup() {
  Serial.begin(115200);   // Debug output to Serial Monitor
  Serial2.begin(9600);    // Receive data from Arduino on Pin 16 (RX2)
  
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected! Ready for Arduino signal.");
}

void loop() {
  // Check if Arduino has sent a message
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();
    
    Serial.print("Arudino Signal: ");
    Serial.println(incoming);
    
    // Check for the "DONE" command from Arduino
    // Format: DONE:TYPE:ID:NAME:PRICE:QTY
    if (incoming.startsWith("DONE:")) {
      Serial.println(">>> TRANSACTION DETECTED! Sending to Supabase...");
      
      // We parse the message (e.g., DONE:pen:2:Standard Ballpen:10.0:1)
      int typeIdx = incoming.indexOf(':', 0) + 1;
      int idIdx = incoming.indexOf(':', typeIdx) + 1;
      int nameIdx = incoming.indexOf(':', idIdx) + 1;
      int priceIdx = incoming.indexOf(':', nameIdx) + 1;
      int qtyIdx = incoming.indexOf(':', priceIdx) + 1;
      
      String type = incoming.substring(typeIdx, idIdx - 1);
      String id = incoming.substring(idIdx, nameIdx - 1);
      String name = incoming.substring(nameIdx, priceIdx - 1);
      String price = incoming.substring(priceIdx, qtyIdx - 1);
      String qty = incoming.substring(qtyIdx);
      
      // LOG IT!
      logTransaction(type, id, name, price, qty);
    }
  }
}

void logTransaction(String type, String id, String name, String price, String qty) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(30);

  HTTPClient http;
  String url = supabase_url + "/rest/v1/sales_transactions";
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");

  // Create JSON Payload
  // We use the brand_id column for the item's ID in settings table
  String jsonBody = "{\"item_type\":\"" + type + "\", \"brand_id\":" + id + ", \"amount_paid\":" + price + ", \"qty_dispensed\":" + qty + "}";

  int httpCode = http.POST(jsonBody);
  
  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("SUCCESS! Transation logged to Supabase.");
    Serial.println("Stock in paper/pen_settings will be subtracted automatically by the Trigger.");
    Serial.println("\nGo check your 'sales_transactions' table in the Dashboard.");
  } else {
    Serial.println("Logging Failed! Error: " + http.errorToString(httpCode));
    Serial.println("Response: " + http.getString());
  }
  http.end();
}

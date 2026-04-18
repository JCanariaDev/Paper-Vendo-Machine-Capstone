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
const char* ssid = "realme C3";
const char* password = "lancelot";

//const char* ssid = "HericoSnap 2.4G";
//const char* password = "HericoAA24";

// --- SUPABASE CONFIG ---
const String supabase_url = "https://jowpzdynbdeznuvohrpx.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Impvd3B6ZHluYmRlem51dm9ocnB4Iiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYxMTExNDYsImV4cCI6MjA5MTY4NzE0Nn0.plD8ehYQsBgzfXrXBHJpqHanQF5GPKYlM53I1t3wfO0";

void setup() {
  Serial.begin(115200);   // Debug output to Serial Monitor
  
  // Explicitly tell the ESP32 to use Pin 16 as the "Ear" (RX)
  Serial2.begin(9600, SERIAL_8N1, 16, 17); 
  Serial2.setTimeout(100); // Wait a bit longer for the full message
  
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
    
    // Check for "REQUEST" from Arduino (Handshake start)
    // Format: REQ:TYPE:ID:COINS
    if (incoming.startsWith("REQ:")) {
      int typeIdx = incoming.indexOf(':', 0) + 1;
      int idIdx = incoming.indexOf(':', typeIdx) + 1;
      int coinIdx = incoming.indexOf(':', idIdx) + 1;
      
      String type = incoming.substring(typeIdx, idIdx - 1);
      String id = incoming.substring(idIdx, coinIdx - 1);
      float userCoins = incoming.substring(coinIdx).toFloat();
      
      Serial.print(">>> CLOUD CHECK for Item ID: "); Serial.println(id);
      fetchItemData(type, id, userCoins);
    }
    
    // Check for the "DONE" command from Arduino (Final log)
    if (incoming.startsWith("DONE:")) {
      // ... (Existing parsing code)
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
      
      logTransaction(type, id, name, price, qty);
    }
  }
}

void fetchItemData(String type, String id, float userCoins) {
  WiFiClientSecure client;
  client.setInsecure();
  
  HTTPClient http;
  String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
  
  // Specific columns for each table to avoid SQL errors
  String selectCols = (type == "paper") ? "cost_per_unit,sheets_per_unit,paper_size" : "cost_per_unit,item_name";
  String url = supabase_url + "/rest/v1/" + table + "?id=eq." + id + "&select=" + selectCols;
  
  Serial.print(">>> Fetching from: "); Serial.println(url);
  
  http.begin(client, url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  
  int httpCode = http.GET();
  Serial.print(">>> Cloud Response Code: "); Serial.println(httpCode);
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.print(">>> Data Received: "); Serial.println(payload);
    
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    if (doc.size() > 0) {
      float cost = doc[0]["cost_per_unit"];
      int sheets = (type == "paper") ? (int)doc[0]["sheets_per_unit"] : 1;
      String name = (type == "paper") ? doc[0]["paper_size"].as<String>() : doc[0]["item_name"].as<String>();
      
      if (userCoins >= cost) {
        Serial.println(">>> CREDIT OK!");
        Serial2.print("DISPENSE:");
        Serial2.print(sheets); Serial2.print(":");
        Serial2.print(cost); Serial2.print(":");
        Serial2.println(name);
      } else {
        Serial.println(">>> CREDIT TOO LOW");
        Serial2.println("ERR:LOW_CREDIT");
      }
    } else {
      Serial.println(">>> ITEM NOT FOUND");
      Serial2.println("ERR:ID_NOT_FOUND");
    }
  } else {
    Serial.print(">>> CLOUD ERROR: "); Serial.println(http.errorToString(httpCode));
    Serial2.println("ERR:CLOUD_FAIL");
  }
  http.end();
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
  // We include paper_size so the Paper Trigger can find the right row
  String jsonBody = "{\"item_type\":\"" + type + "\", \"brand_id\":" + id + ", \"paper_size\":\"" + name + "\", \"amount_paid\":" + price + ", \"qty_dispensed\":" + qty + "}";

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

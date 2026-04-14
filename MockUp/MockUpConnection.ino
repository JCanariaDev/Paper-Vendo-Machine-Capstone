#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h>

/*
  MockUpConnection.ino (CRUD V2)
  This code performs a larger test: 
  1. READ - Fetches ALL Paper and Ballpen inventory.
  2. UPDATE - Modifies a specific Paper (Standard Crosswise) and a Ballpen (Budget).
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
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // --- START THE LARGE TEST ---
  Serial.println("\n--- [STARTING LARGE CRUD TEST] ---");
  
  readAllInventory();   // (R)eading everything
  updateItems();        // (U)pdating specific rows
  
  Serial.println("\n--- [ALL TESTS COMPLETED] ---");
}

void loop() {}

// --- [READ] FETCH EVERYTHING ---
void readAllInventory() {
  Serial.println("\n[1] FETCHING ALL RECORDS...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // FETCH PAPERS
  Serial.println("\n--- PAPER STOCK ---");
  http.begin(client, supabase_url + "/rest/v1/paper_settings?select=id,brand_name,paper_size,cost_per_unit,current_stock");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  
  int code = http.GET();
  if (code == 200) {
    String res = http.getString();
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, res);
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      Serial.printf("ID: %d | %s (%s) | P%.2f | Stock: %d\n", 
                    obj["id"].as<int>(), obj["brand_name"].as<const char*>(), 
                    obj["paper_size"].as<const char*>(), obj["cost_per_unit"].as<float>(), 
                    obj["current_stock"].as<int>());
    }
  }
  http.end();

  // FETCH BALLPENS
  Serial.println("\n--- BALLPEN STOCK ---");
  http.begin(client, supabase_url + "/rest/v1/ballpen_settings?select=id,item_name,cost_per_unit,current_stock");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  
  code = http.GET();
  if (code == 200) {
    String res = http.getString();
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, res);
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      Serial.printf("ID: %d | %s | P%.2f | Stock: %d\n", 
                    obj["id"].as<int>(), obj["item_name"].as<const char*>(), 
                    obj["cost_per_unit"].as<float>(), obj["current_stock"].as<int>());
    }
  }
  http.end();
}

// --- [UPDATE] MODIFY SPECIFIC ROWS ---
void updateItems() {
  Serial.println("\n[2] PERFORMING UPDATES...");
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // A. UPDATE PAPER: Standard Crosswise (Typically ID 6)
  // We change its price to 2.50 and stock to 99 as a test
  Serial.println("Updating 'Standard Crosswise Paper' to P2.50...");
  http.begin(client, supabase_url + "/rest/v1/paper_settings?brand_name=eq.Standard%20Brand%20(Yellow)&paper_size=eq.crosswise");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  
  String paperBody = "{\"cost_per_unit\": 2.50, \"current_stock\": 99, \"sheets_per_unit\": 5}";
  int code = http.PATCH(paperBody);
  if (code >= 200 && code < 300) Serial.println(">>> SUCCESS: Paper Updated!");
  else Serial.println(">>> ERROR updating Paper: " + String(code));
  http.end();

  // B. UPDATE BALLPEN: Budget Ballpen (Typically ID 1)
  // We change its price to 6.00 and stock to 45
  Serial.println("Updating 'Budget Ballpen' to P6.00...");
  http.begin(client, supabase_url + "/rest/v1/ballpen_settings?item_name=eq.Budget%20Ballpen");
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  
  String penBody = "{\"cost_per_unit\": 6.00, \"current_stock\": 45}";
  code = http.PATCH(penBody);
  if (code >= 200 && code < 300) Serial.println(">>> SUCCESS: Ballpen Updated!");
  else Serial.println(">>> ERROR updating Ballpen: " + String(code));
  http.end();
}

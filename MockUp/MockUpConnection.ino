/*
  Supabase Connection MockUp Code
  Author: Antigravity
  Description: Simple test to verify ESP32 -> Supabase direct communication.
  
  LIBRARIES NEEDED (Library Manager):
  1. ArduinoJson (by Benoit Blanchon)
  2. HTTPClient (Built-in)
  3. WiFi (Built-in)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- WIFI CONFIG (CHANGE THESE) ---
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// --- SUPABASE CONFIG ---
const String supabase_url = "https://iqbieobtvrkmfjoenwrq.supabase.co";
const String supabase_key = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImlxYmllb2J0dnJrbWZqb2Vud3JxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3NTE1MDMsImV4cCI6MjA4ODMyNzUwM30.bBldTEWeaw3LLyUMtDWwDML3uKL_ofV7sRKd6JrfMZo";

void setup() {
  Serial.begin(115200);
  
  // 1. Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 2. Perform Single Tests
  testReadData();    // Test if we can SEE the inventory
  testWriteData();   // Test if we can UPDATE the heartbeat
}

void loop() {
  // Keep empty for mock test
}

// --- TEST 1: READ DATA ---
void testReadData() {
  Serial.println("\n[TEST 1] Reading Paper Inventory...");
  
  HTTPClient http;
  String url = supabase_url + "/rest/v1/paper_settings?select=brand_name,current_stock&limit=1";
  
  http.begin(url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);

  int httpCode = http.GET();
  
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Response Code: " + String(httpCode));
    Serial.println("Data: " + payload);
    
    // Parse JSON
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String brand = doc[0]["brand_name"];
    int stock = doc[0]["current_stock"];
    
    Serial.println("SUCCESS! Found " + brand + " with stock: " + String(stock));
  } else {
    Serial.println("Error on HTTP request: " + String(httpCode));
  }
  http.end();
}

// --- TEST 2: WRITE DATA ---
void testWriteData() {
  Serial.println("\n[TEST 2] Updating Machine Heartbeat...");
  
  HTTPClient http;
  // We use PATCH to update the existing 'last_heartbeat' row in machine_status
  String url = supabase_url + "/rest/v1/machine_status?status_key=eq.last_heartbeat";
  
  http.begin(url);
  http.addHeader("apikey", supabase_key);
  http.addHeader("Authorization", "Bearer " + supabase_key);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=representation");

  // Create update payload
  String jsonBody = "{\"status_value\": \"Testing_MockUp_Success\"}";

  int httpCode = http.PATCH(jsonBody);
  
  if (httpCode >= 200 && httpCode < 300) {
    Serial.println("SUCCESS! Heartbeat updated in Supabase.");
    Serial.println("Go check your 'machine_status' table in the Dashboard.");
  } else {
    Serial.println("Failed to update. Error: " + String(httpCode));
    Serial.println("Response: " + http.getString());
  }
  http.end();
}

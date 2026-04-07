

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// --- WIFI CONFIGURATION ---
const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PASSWORD";

// --- SUPABASE CONFIGURATION ---
const String SUPABASE_URL = "https://iqbieobtvrkmfjoenwrq.supabase.co/rest/v1/";
const String SUPABASE_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6ImlxYmllb2J0dnJrbWZqb2Vud3JxIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzI3NTE1MDMsImV4cCI6MjA4ODMyNzUwM30.bBldTEWeaw3LLyUMtDWwDML3uKL_ofV7sRKd6JrfMZo";

#define RXD2 16
#define TXD2 17

unsigned long lastHeartbeat = 0;

void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    connectWiFi();
    Serial.println("ESP32 Cloud Ready");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) connectWiFi();

    if (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        msg.trim();
        if (msg.length() > 0) {
            Serial.println("Arduino -> ESP32: " + msg);
            if (msg.startsWith("SENS:")) processSensorReport(msg);
            else if (msg.length() > 0) handleArduinoProtocol(msg);
        }
    }

    if (millis() - lastHeartbeat > 30000) { 
        updateMachineStatus("Running");
        lastHeartbeat = millis();
    }
}

// --------------------------------------------------------------------------------
// PROTOCOL HANDLERS
// --------------------------------------------------------------------------------

void processSensorReport(String msg) {
    // Protocol: SENS:Q:CROSS:LENGTH:WHOLE:PEN
    // 1=Empty, 0=Good
    int values[5];
    int count = 0;
    int startIdx = 5;
    for (int i = 0; i < 5; i++) {
        int nextColon = msg.indexOf(':', startIdx);
        if (nextColon == -1) nextColon = msg.length();
        values[i] = msg.substring(startIdx, nextColon).toInt();
        startIdx = nextColon + 1;
    }

    // Update 4 paper slots (Assuming IDs 1,2,3,4)
    updatePhysicalStatus("paper", 1, values[0] == 1 ? "Empty" : "Good");
    updatePhysicalStatus("paper", 2, values[1] == 1 ? "Empty" : "Good");
    updatePhysicalStatus("paper", 3, values[2] == 1 ? "Empty" : "Good");
    updatePhysicalStatus("paper", 4, values[3] == 1 ? "Empty" : "Good");
    // Update 1 pen slot (Small modification: Assuming ballpen ID is 1)
    updatePhysicalStatus("ballpen", 1, values[4] == 1 ? "Empty" : "Good");
}

void handleArduinoProtocol(String msg) {
    if (msg.startsWith("REQ:")) {
        if (msg.substring(4) == "PEN") {
            requestComputation("ballpen", 1, "");
        } else {
            int firstColon = msg.indexOf(':');
            int secondColon = msg.indexOf(':', firstColon + 1);
            int bid = msg.substring(firstColon+1, secondColon).toInt();
            String size = msg.substring(secondColon+1);
            requestComputation("paper", bid, size);
        }
    } 
    else if (msg.startsWith("DONE:")) {
        registerTransaction(msg);
    }
}

// --------------------------------------------------------------------------------
// CLOUD OPERATIONS (SUPABASE)
// --------------------------------------------------------------------------------

void updatePhysicalStatus(String type, int id, String status) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String table = (type == "paper") ? "paper_settings" : "ballpen_settings";
    String url = SUPABASE_URL + table + "?id=eq." + String(id);
    
    http.begin(client, url);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY);
    http.addHeader("Content-Type", "application/json");

    String json = "{\"physical_status\":\"" + status + "\"}";
    http.PATCH(json);
    http.end();
}

void requestComputation(String type, int id, String size) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String url;
    if (type == "paper") {
        url = SUPABASE_URL + "paper_settings?id=eq." + String(id) + "&paper_size=eq." + size + "&select=sheets_per_unit,cost_per_unit";
    } else {
        url = SUPABASE_URL + "ballpen_settings?id=eq." + String(id) + "&select=cost_per_unit";
    }

    http.begin(client, url);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY);
    
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(512);
        deserializeJson(doc, payload);
        
        if (doc.size() > 0) {
            if (type == "paper") {
                int sheets = doc[0]["sheets_per_unit"];
                int cost = doc[0]["cost_per_unit"];
                Serial2.println("DISP:" + String(sheets) + ":" + String(cost));
            } else {
                int cost = doc[0]["cost_per_unit"];
                Serial2.println("DISP_PEN:" + String(cost));
            }
        } else {
            Serial2.println("ERR:No Stock Data");
        }
    } else {
        Serial2.println("ERR:Cloud Error");
    }
    http.end();
}

void registerTransaction(String msg) {
    // Protocol: DONE:TYPE:ID:SIZE:AMT:QTY (for paper) OR DONE:pen:ID:AMT:QTY (for pen)
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    int parts[7];
    int count = 0;
    int pos = 0;
    while ((pos = msg.indexOf(':', pos)) != -1 && count < 7) {
        parts[count++] = pos;
        pos++;
    }

    String type = msg.substring(parts[0]+1, parts[1]);
    int id = msg.substring(parts[1]+1, parts[2]).toInt();
    
    String size = "";
    float cost = 0;
    int qty = 0;

    if (type == "paper") {
        size = msg.substring(parts[2]+1, parts[3]);
        cost = msg.substring(parts[3]+1, parts[4]).toFloat();
        qty = msg.substring(parts[4]+1).toInt();
    } else { // Pen doesn't send size
        cost = msg.substring(parts[2]+1, parts[3]).toFloat();
        qty = msg.substring(parts[3]+1).toInt();
    }

    http.begin(client, SUPABASE_URL + "sales_transactions");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(256);
    doc["item_type"] = type;
    doc["brand_id"] = id;
    if (type == "paper") doc["paper_size"] = size;
    doc["amount_paid"] = cost;
    doc["qty_dispensed"] = qty;

    String payload;
    serializeJson(doc, payload);
    http.POST(payload);
    http.end();
}

void updateMachineStatus(String s) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    String url = SUPABASE_URL + "machine_status?status_key=eq.last_heartbeat";
    http.begin(client, url);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + SUPABASE_KEY);
    http.addHeader("Content-Type", "application/json");

    String timeJson = "{\"status_value\":\"" + String(millis()) + "\"}"; 
    http.PATCH(timeJson);
    http.end();
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected");
}

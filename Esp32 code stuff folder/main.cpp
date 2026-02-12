/*
 * Paper Vendo Machine - ESP32 Code
 * Single ESP32 Architecture
 * Handles: Status updates (is_running), Paper computation based on brand and size
 * Communicates with Arduino via Serial2
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ============================================
// CONFIGURATION
// ============================================
const char* ssid = "YOUR_WIFI_SSID"; //Realme C3
const char* password = "YOUR_WIFI_PASSWORD"; //Lancelot

// API Server - CHANGE TO YOUR SERVER IP
// Example: 192.168.1.10 (your XAMPP server PC)
#define SERVER_IP "192.168.1.100"

// We'll build the full URL at runtime using this base
const String API_BASE = String("http://") + SERVER_IP + "/Paper%20Vendo%20Machine%20Capstone/Api%20Folder/api.php";

// UART to Arduino (Serial2)
#define RXD2 16
#define TXD2 17

// ============================================
// GLOBALS
// ============================================
struct PaperSetting {
    int id;
    String brand_name;
    String paper_size;
    float cost_per_unit;
    int sheets_per_unit;
    int current_stock;
};

PaperSetting cachedSettings[20]; // Max 20 combinations (brands x sizes)
int settingsCount = 0;

unsigned long lastHeartbeat = 0;
unsigned long lastSettingsFetch = 0;
const unsigned long HEARTBEAT_INTERVAL = 30000;  // 30 seconds
const unsigned long SETTINGS_REFRESH_INTERVAL = 60000; // 60 seconds

String currentBrandRequest = "";
String currentSizeRequest = "";

// ============================================
// SETUP
// ============================================
void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    
    Serial.println("\n=== Paper Vendo ESP32 Starting ===");
    
    connectWiFi();
    fetchAllSettings(); // Initial fetch
    
    // Send initial status
    sendStatus("is_running", "Running");
    
    Serial.println("ESP32 Ready!");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
    // 1. Maintain WiFi Connection
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        connectWiFi();
    }

    // 2. Check for Messages from Arduino
    if (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        msg.trim();
        if (msg.length() > 0) {
            Serial.println("Arduino: " + msg);
            processArduinoMessage(msg);
        }
    }

    // 3. Heartbeat (Every 30s)
    if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
        sendStatus("is_running", "Running");
        lastHeartbeat = millis();
    }

    // 4. Refresh Settings Cache (Every 60s)
    if (millis() - lastSettingsFetch > SETTINGS_REFRESH_INTERVAL) {
        fetchAllSettings();
        lastSettingsFetch = millis();
    }
    
    delay(10); // Small delay to prevent watchdog issues
}

// ============================================
// WIFI CONNECTION
// ============================================
void connectWiFi() {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi Connection Failed!");
    }
}

// ============================================
// FETCH ALL SETTINGS (Cache for fast lookup)
// ============================================
void fetchAllSettings() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot fetch settings - WiFi not connected");
        return;
    }

    HTTPClient http;
    String url = API_BASE + "?action=get_all_settings";
    
    Serial.println("Fetching settings from: " + url);
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            Serial.println("Response: " + payload);
            
            DynamicJsonDocument doc(4096);
            DeserializationError error = deserializeJson(doc, payload);

            if (!error && doc["success"] == true) {
                JsonArray data = doc["data"];
                settingsCount = 0;
                
                for (JsonObject item : data) {
                    if (settingsCount < 20) {
                        cachedSettings[settingsCount].id = item["id"];
                        cachedSettings[settingsCount].brand_name = item["brand_name"].as<String>();
                        cachedSettings[settingsCount].paper_size = item["paper_size"].as<String>();
                        cachedSettings[settingsCount].cost_per_unit = item["cost_per_unit"];
                        cachedSettings[settingsCount].sheets_per_unit = item["sheets_per_unit"];
                        cachedSettings[settingsCount].current_stock = item["current_stock"];
                        settingsCount++;
                    }
                }
                Serial.println("Settings cached: " + String(settingsCount) + " items");
            } else {
                Serial.println("JSON Parse Error or API Error");
            }
        } else {
            Serial.println("HTTP Error: " + String(httpCode));
        }
    } else {
        Serial.println("HTTP Request Failed");
    }
    
    http.end();
}

// ============================================
// GET PAPER COMPUTATION (Brand + Size)
// ============================================
bool getPaperComputation(int brand_id, String paper_size, PaperSetting& result) {
    // First check cache
    for (int i = 0; i < settingsCount; i++) {
        if (cachedSettings[i].id == brand_id && cachedSettings[i].paper_size == paper_size) {
            if (cachedSettings[i].current_stock >= cachedSettings[i].sheets_per_unit) {
                result = cachedSettings[i];
                return true;
            } else {
                Serial.println("Insufficient stock in cache");
                return false;
            }
        }
    }
    
    // If not in cache, fetch from API
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot fetch computation - WiFi not connected");
        return false;
    }

    HTTPClient http;
    String url = API_BASE + "?action=get_paper_computation&brand_id=" + String(brand_id) + "&paper_size=" + paper_size;
    
    http.begin(url);
    http.setTimeout(5000);
    
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, payload);

        if (!error && doc["success"] == true) {
            result.id = doc["brand_id"];
            result.brand_name = doc["brand_name"].as<String>();
            result.paper_size = doc["paper_size"].as<String>();
            result.cost_per_unit = doc["cost_per_unit"];
            result.sheets_per_unit = doc["sheets_per_unit"];
            result.current_stock = doc["current_stock"];
            http.end();
            return true;
        }
    }
    
    http.end();
    return false;
}

// ============================================
// PROCESS ARDUINO MESSAGES
// ============================================
void processArduinoMessage(String msg) {
    // Protocol:
    // "REQ:BRAND_ID:SIZE" -> Arduino asks what to dispense
    // "DONE:BRAND_ID:SIZE:AMOUNT:SHEETS" -> Arduino reports success
    // "ERR:ERROR_MESSAGE" -> Arduino reports error

    if (msg.startsWith("REQ:")) {
        // Format: REQ:BRAND_ID:SIZE
        // e.g., REQ:1:1/4 or REQ:2:1_whole
        int firstColon = msg.indexOf(':');
        int secondColon = msg.indexOf(':', firstColon + 1);
        
        if (secondColon > 0) {
            int brandId = msg.substring(firstColon + 1, secondColon).toInt();
            String paperSize = msg.substring(secondColon + 1);
            
            Serial.println("Request: Brand=" + String(brandId) + ", Size=" + paperSize);
            
            PaperSetting setting;
            if (getPaperComputation(brandId, paperSize, setting)) {
                // Reply to Arduino: "DISP:SHEETS:COST"
                String reply = "DISP:" + String(setting.sheets_per_unit) + ":" + String(setting.cost_per_unit);
                Serial2.println(reply);
                Serial.println("Replied: " + reply);
                
                // Store for transaction logging
                currentBrandRequest = String(brandId);
                currentSizeRequest = paperSize;
            } else {
                // Insufficient stock or error
                Serial2.println("ERR:InsufficientStock");
                Serial.println("Error: Insufficient stock or brand/size not found");
            }
        }
    }
    else if (msg.startsWith("DONE:")) {
        // Format: DONE:BRAND_ID:SIZE:AMOUNT:SHEETS
        // e.g., DONE:1:1/4:1.00:4
        int colons[4];
        int colonCount = 0;
        
        for (int i = 0; i < msg.length() && colonCount < 4; i++) {
            if (msg.charAt(i) == ':') {
                colons[colonCount++] = i;
            }
        }
        
        if (colonCount >= 4) {
            int brandId = msg.substring(colons[0] + 1, colons[1]).toInt();
            String paperSize = msg.substring(colons[1] + 1, colons[2]);
            float amount = msg.substring(colons[2] + 1, colons[3]).toFloat();
            int sheets = msg.substring(colons[3] + 1).toInt();
            
            sendTransaction(brandId, paperSize, amount, sheets);
        }
    }
    else if (msg.startsWith("ERR:")) {
        // Format: ERR:ERROR_MESSAGE
        String errorMsg = msg.substring(4);
        sendStatus("current_error", errorMsg);
        Serial.println("Error from Arduino: " + errorMsg);
    }
}

// ============================================
// SEND STATUS UPDATE
// ============================================
void sendStatus(String key, String value) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url = API_BASE + "?action=update_status";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(256);
    doc["is_running"] = value;
    
    String json;
    serializeJson(doc, json);
    
    int httpCode = http.POST(json);
    
    if (httpCode > 0) {
        Serial.println("Status updated: " + key + " = " + value);
    } else {
        Serial.println("Failed to update status: " + String(httpCode));
    }
    
    http.end();
}

// ============================================
// SEND TRANSACTION
// ============================================
void sendTransaction(int brand_id, String paper_size, float amount, int sheets) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Cannot send transaction - WiFi not connected");
        return;
    }

    HTTPClient http;
    String url = API_BASE + "?action=register_transaction";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(512);
    doc["brand_id"] = brand_id;
    doc["paper_size"] = paper_size;
    doc["amount_paid"] = amount;
    doc["sheets_dispensed"] = sheets;
    
    String json;
    serializeJson(doc, json);
    
    Serial.println("Sending transaction: " + json);
    
    int httpCode = http.POST(json);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("Transaction registered successfully");
        } else {
            Serial.println("Transaction failed: " + String(httpCode));
        }
    } else {
        Serial.println("HTTP Request Failed");
    }
    
    http.end();
}

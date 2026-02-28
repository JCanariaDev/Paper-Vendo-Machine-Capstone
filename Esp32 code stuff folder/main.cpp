/*
 * Paper Vendo Machine - ESP32 Code
 * Syncs Physical Sensor Data to Web API
 */

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
// --- LOCAL CONFIGURATION (XAMPP) ---
#define SERVER_IP "192.168.1.100"
const String API_BASE = String("http://") + SERVER_IP + "/Paper%20Vendo%20Machine%20Capstone/Api%20Folder/api.php";

// --- HOSTING CONFIGURATION (Uncomment to use) ---
/*
const String API_BASE = "https://yourdomain.infinityfreeapp.com/Api%20Folder/api.php";
*/

#define RXD2 16
#define TXD2 17

struct PaperSetting {
    int id;
    String size;
};

PaperSetting paperMap[4]; // 0:1/4, 1:cross, 2:length, 3:whole

unsigned long lastHeartbeat = 0;
void setup() {
    Serial.begin(115200);
    Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
    connectWiFi();
    
    // Hardcode map for sensors to database IDs (Adjust based on your DB)
    paperMap[0] = {1, "1/4"};
    paperMap[1] = {2, "crosswise"};
    paperMap[2] = {3, "lengthwise"};
    paperMap[3] = {4, "1_whole"};
    
    Serial.println("ESP32 Ready");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED) connectWiFi();

    if (Serial2.available()) {
        String msg = Serial2.readStringUntil('\n');
        msg.trim();
        if (msg.startsWith("SENS:")) processSensorReport(msg);
        else if (msg.length() > 0) handleArduinoProtocol(msg);
    }

    if (millis() - lastHeartbeat > 30000) {
        updateMachineStatus("Running");
        lastHeartbeat = millis();
    }
}

void processSensorReport(String msg) {
    // SENS:Q:CROSS:LENGTH:WHOLE:PEN
    // 1=Empty, 0=Good
    int values[5];
    int count = 0;
    int lastColon = 0;
    for (int i = 5; i < msg.length() && count < 5; i++) {
        if (msg[i] == ':' || i == msg.length()-1) {
            values[count++] = msg.substring(lastColon ? lastColon + 1 : 5, (msg[i] == ':') ? i : i + 1).toInt();
            lastColon = i;
        }
    }

    if (count == 5) {
        // Sync 4 paper slots
        for (int i = 0; i < 4; i++) {
            updatePhysicalStatus("paper", paperMap[i].id, values[i] == 1 ? "Empty" : "Good");
        }
        // Sync 1 pen slot
        updatePhysicalStatus("ballpen", 1, values[4] == 1 ? "Empty" : "Good");
    }
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
        // Registration logic (Simplified)
        HTTPClient http;
        http.begin(API_BASE + "?action=register_transaction");
        http.addHeader("Content-Type", "application/json");
        // ... (Send JSON based on message)
        http.end();
    }
}

void updatePhysicalStatus(String type, int id, String status) {
    HTTPClient http;
    
    // --- FOR HTTPS/SSL (Uncomment if using Hosting) ---
    /*
    WiFiClientSecure client;
    client.setInsecure(); // Skips certificate validation (good for testing)
    http.begin(client, API_BASE + "?action=update_sensor_status");
    */
    
    // --- FOR LOCAL HTTP ---
    http.begin(API_BASE + "?action=update_sensor_status");
    
    http.addHeader("Content-Type", "application/json");
    String json = "{\"item_type\":\"" + type + "\",\"item_id\":" + String(id) + ",\"status\":\"" + status + "\"}";
    http.POST(json);
    http.end();
}

void requestComputation(String type, int id, String size) {
    HTTPClient http;
    String url = API_BASE + (type == "paper" ? "?action=get_paper_computation&brand_id=" : "?action=get_ballpen_computation&id=") + String(id);
    if (type == "paper") url += "&paper_size=" + size;
    
    http.begin(url);
    if (http.GET() == 200) {
        String payload = http.getString();
        DynamicJsonDocument doc(512);
        deserializeJson(doc, payload);
        if (doc["success"] == true) {
            String replyString;
            if (type == "paper") replyString = "DISP:" + String(doc["sheets_per_unit"].as<int>()) + ":" + String(doc["cost_per_unit"].as<int>());
            else replyString = "DISP_PEN:" + String(doc["cost_per_unit"].as<int>());
            Serial2.println(replyString);
        }
    }
    http.end();
}

void updateMachineStatus(String s) {
    HTTPClient http;
    http.begin(API_BASE + "?action=update_status");
    http.addHeader("Content-Type", "application/json");
    http.POST("{\"is_running\":\"" + s + "\"}");
    http.end();
}

void connectWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) delay(500);
}

  #include <WiFi.h>

// ESP32 + Mega 2560 Wi-Fi status UART test.
// Uses the same bootstrap Wi-Fi credentials as the active ESP32 sketch.
// UART2: ESP32 RX GPIO16 <- Mega TX1 D18 through a level shifter;
//        ESP32 TX GPIO17 -> Mega RX1 D19; connect grounds together.

const char* WIFI_SSID = "ashid";
const char* WIFI_PASSWORD = "paltankolang";

HardwareSerial& MEGA_SERIAL = Serial2;
const int MEGA_RX_PIN = 16;
const int MEGA_TX_PIN = 17;

const unsigned long STATUS_REPORT_INTERVAL_MS = 2000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 5000;

String megaLine;
bool lastReportedWifiState = false;
bool hasReportedWifiState = false;
unsigned long lastStatusReportAt = 0;
unsigned long lastWifiRetryAt = 0;

void sendWifiStatus(bool connected) {
  const String message = String("WIFI:") + (connected ? "1" : "0");
  MEGA_SERIAL.println(message);
  Serial.print("Sent to Mega: ");
  Serial.println(message);
}

void readMegaUart() {
  while (MEGA_SERIAL.available()) {
    const char incoming = static_cast<char>(MEGA_SERIAL.read());
    if (incoming == '\n') {
      megaLine.trim();
      if (megaLine.length() > 0) {
        Serial.print("Received from Mega: ");
        Serial.println(megaLine);
        if (megaLine == "STATUS?") {
          sendWifiStatus(WiFi.status() == WL_CONNECTED);
        }
      }
      megaLine = "";
    } else if (incoming != '\r' && megaLine.length() < 100) {
      megaLine += incoming;
    } else if (megaLine.length() >= 100) {
      megaLine = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  MEGA_SERIAL.begin(9600, SERIAL_8N1, MEGA_RX_PIN, MEGA_TX_PIN);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("ESP32 Mega Wi-Fi UART test started.");
  Serial.print("Connecting to SSID: ");
  Serial.println(WIFI_SSID);
  Serial.println("UART2: RX GPIO16, TX GPIO17, 9600 baud.");
}

void loop() {
  readMegaUart();

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (!hasReportedWifiState || connected != lastReportedWifiState ||
      millis() - lastStatusReportAt >= STATUS_REPORT_INTERVAL_MS) {
    lastReportedWifiState = connected;
    hasReportedWifiState = true;
    lastStatusReportAt = millis();
    sendWifiStatus(connected);
    if (connected) {
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    }
  }

  if (!connected && millis() - lastWifiRetryAt >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryAt = millis();
    Serial.println("Wi-Fi disconnected; requesting reconnect.");
    WiFi.reconnect();
  }
}

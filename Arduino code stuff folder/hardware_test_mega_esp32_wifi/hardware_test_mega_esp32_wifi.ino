#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

// Mega 2560 + ILI9341 TFT + ESP32 Wi-Fi status UART test.
// TFT SPI pins: MOSI 51, MISO 50, SCK 52, CS 53, DC 48, RESET 49.
// ESP32 UART: Mega TX1 D18 -> ESP32 RX GPIO16 through a level shifter;
//             Mega RX1 D19 <- ESP32 TX GPIO17; connect grounds together.

#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49

const unsigned long STATUS_REQUEST_INTERVAL_MS = 2000;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
String serialLine;
unsigned long lastStatusRequestAt = 0;
unsigned long statusMessageCount = 0;
bool wifiConnected = false;
bool haveWifiStatus = false;

void drawStatusScreen(const char* detail) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 25);
  tft.println("ESP32 WIFI TEST");

  tft.drawFastHLine(20, 60, tft.width() - 40, ILI9341_DARKGREY);
  tft.setTextSize(3);
  tft.setTextColor(wifiConnected ? ILI9341_GREEN : ILI9341_RED);
  tft.setCursor(20, 100);
  tft.println(wifiConnected ? "CONNECTED" : "WAITING");

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 155);
  tft.println(detail);

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_CYAN);
  tft.setCursor(20, 215);
  tft.print("UART status messages: ");
  tft.println(statusMessageCount);
  tft.setCursor(20, 240);
  tft.print("Mega Serial1: 9600 baud, D18/D19");
}

void handleEspMessage(String message) {
  message.trim();
  if (!message.startsWith("WIFI:")) {
    Serial.print("ESP32 UART: ");
    Serial.println(message);
    return;
  }

  const bool nextConnected = message.substring(5).toInt() == 1;
  const bool stateChanged = !haveWifiStatus || nextConnected != wifiConnected;
  wifiConnected = nextConnected;
  haveWifiStatus = true;
  statusMessageCount++;

  Serial.print("Received from ESP32: ");
  Serial.println(message);
  if (stateChanged) {
    drawStatusScreen(wifiConnected ? "ESP32 reports Wi-Fi online" : "ESP32 reports Wi-Fi offline");
  } else {
    tft.fillRect(20, 215, tft.width() - 40, 18, ILI9341_BLACK);
    tft.setTextSize(1);
    tft.setTextColor(ILI9341_CYAN);
    tft.setCursor(20, 215);
    tft.print("UART status messages: ");
    tft.println(statusMessageCount);
  }
}

void readEspUart() {
  while (Serial1.available()) {
    const char incoming = static_cast<char>(Serial1.read());
    if (incoming == '\n') {
      handleEspMessage(serialLine);
      serialLine = "";
    } else if (incoming != '\r' && serialLine.length() < 100) {
      serialLine += incoming;
    } else if (serialLine.length() >= 100) {
      serialLine = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  tft.begin();
  tft.setRotation(2);
  drawStatusScreen("Waiting for ESP32 status");

  Serial.println("Mega ESP32 Wi-Fi display test started.");
  Serial.println("Waiting for WIFI:1 or WIFI:0 on Serial1 (D19 RX1).");
}

void loop() {
  readEspUart();

  if (millis() - lastStatusRequestAt >= STATUS_REQUEST_INTERVAL_MS) {
    lastStatusRequestAt = millis();
    Serial1.println("STATUS?");
  }
}

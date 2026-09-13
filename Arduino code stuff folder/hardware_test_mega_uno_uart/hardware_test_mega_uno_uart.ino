#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

/*
  MEGA -> UNO UART TEST WITH ILI9341 TFT

  Uses the same pins as the main Mega sketch:
    Mega D16 / TX2 -> Uno D0 / RX
    Mega D17 / RX2 <- Uno D1 / TX
    Mega GND       -> Uno GND
    UART speed     = 9600 baud

  TFT pins:
    TFT_CS    D53
    TFT_DC    D48
    TFT_RST   D49
    TOUCH_CS  D47
    SPI       D50/D51/D52

  Touch calibration matches the main Mega sketch.
*/

#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

#define TS_MINX 328
#define TS_MAXX 3531
#define TS_MINY 336
#define TS_MAXY 3434

const int UART_BAUD_RATE = 9600;
const int BUTTON_X = 35;
const int BUTTON_Y = 250;
const int BUTTON_W = 170;
const int BUTTON_H = 50;
const unsigned long TOUCH_DEBOUNCE_MS = 250;
const unsigned long ACK_TIMEOUT_MS = 1500;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touchscreen(TOUCH_CS);

unsigned long touchDebounceUntil = 0;
unsigned long waitingForAckSince = 0;
bool waitingForAck = false;

void drawScreen(const String &message) {
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(message.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, 105);
  tft.print(message);

  tft.fillRoundRect(BUTTON_X, BUTTON_Y, BUTTON_W, BUTTON_H, 8, ILI9341_BLUE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  const String buttonLabel = "SEND SIGNAL";
  tft.getTextBounds(buttonLabel.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setCursor((tft.width() - w) / 2, BUTTON_Y + (BUTTON_H - h) / 2);
  tft.print(buttonLabel);
}

void sendSignalToUno() {
  Serial2.println(F("SIGNAL"));
  waitingForAck = true;
  waitingForAckSince = millis();
  drawScreen("Signal sent");
  Serial.println(F("Sent SIGNAL to Uno; waiting for acknowledgment."));
}

void handleUnoMessages() {
  while (Serial2.available() > 0) {
    String message = Serial2.readStringUntil('\n');
    message.trim();
    if (message == "ACK_SIGNAL") {
      waitingForAck = false;
      drawScreen("UNO OK");
      Serial.println(F("Uno acknowledgment received."));
    }
  }
}

void handleTouch() {
  if (millis() < touchDebounceUntil || !touchscreen.touched()) return;

  TS_Point point = touchscreen.getPoint();
  int x = map(point.x, TS_MINX, TS_MAXX, 0, tft.width());
  int y = map(point.y, TS_MINY, TS_MAXY, 0, tft.height());
  x = constrain(x, 0, tft.width() - 1);
  y = constrain(y, 0, tft.height() - 1);
  touchDebounceUntil = millis() + TOUCH_DEBOUNCE_MS;

  if (x >= BUTTON_X && x <= BUTTON_X + BUTTON_W &&
      y >= BUTTON_Y && y <= BUTTON_Y + BUTTON_H) {
    sendSignalToUno();
  }
}

void setup() {
  Serial.begin(115200);
  Serial2.begin(UART_BAUD_RATE);

  tft.begin();
  tft.setRotation(0);
  touchscreen.begin();
  touchscreen.setRotation(0);

  drawScreen("Signal");
  Serial.println(F("Mega-Uno UART test ready."));
  Serial.println(F("Touch SEND SIGNAL to send SIGNAL to the Uno."));
}

void loop() {
  handleUnoMessages();
  handleTouch();

  if (waitingForAck && millis() - waitingForAckSince >= ACK_TIMEOUT_MS) {
    waitingForAck = false;
    drawScreen("No response");
    Serial.println(F("No acknowledgment received from Uno."));
  }
}

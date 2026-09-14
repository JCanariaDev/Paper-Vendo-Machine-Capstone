#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

/*
  MEGA TEST CONTROLLER: BALLPEN HARDWARE MOVED TO A SEPARATE UNO

  This Mega test intentionally does NOT use:
    - ULN2003 / ballpen stepper motor
    - Ballpen IR sensor
    - Passive buzzer
    - Status LEDs

  It keeps the production TFT pins and Mega-to-Uno UART pins:
    Mega D14 / TX3 -> Ballpen Uno D0 / RX
    Mega D15 / RX3 <- Ballpen Uno D1 / TX
    Mega GND       <-> Ballpen Uno GND
    UART speed       9600 baud

  TFT pins match the main Mega sketch:
    TFT_CS D53, TFT_DC D48, TFT_RST D49, TOUCH_CS D47
    Hardware SPI: D50 MISO, D51 MOSI, D52 SCK
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
const int TEST_BUTTON_X = 25;
const int TEST_BUTTON_Y = 210;
const int TEST_BUTTON_W = 190;
const int TEST_BUTTON_H = 55;
const int STOP_BUTTON_X = 25;
const int STOP_BUTTON_Y = 275;
const int STOP_BUTTON_W = 190;
const int STOP_BUTTON_H = 35;
const unsigned long TOUCH_DEBOUNCE_MS = 250;
const unsigned long RESPONSE_TIMEOUT_MS = 7000;

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touchscreen(TOUCH_CS);

unsigned long touchDebounceUntil = 0;
unsigned long responseWaitStartedAt = 0;
bool waitingForUno = false;

void drawButton(int x, int y, int w, int h, uint16_t color, const char* label, int textSize) {
  tft.fillRoundRect(x, y, w, h, 8, color);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(textSize);
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &textW, &textH);
  tft.setCursor(x + (w - textW) / 2, y + (h - textH) / 2);
  tft.print(label);
}

void drawScreen(const String &status) {
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds(status.c_str(), 0, 0, &x1, &y1, &textW, &textH);
  tft.setCursor((tft.width() - textW) / 2, 80);
  tft.print(status);

  drawButton(TEST_BUTTON_X, TEST_BUTTON_Y, TEST_BUTTON_W, TEST_BUTTON_H,
             ILI9341_BLUE, "TEST BALLPEN", 2);
  drawButton(STOP_BUTTON_X, STOP_BUTTON_Y, STOP_BUTTON_W, STOP_BUTTON_H,
             ILI9341_RED, "STOP", 1);
}

void startBallpenTest() {
  Serial3.println(F("BALLPEN_TEST"));
  waitingForUno = true;
  responseWaitStartedAt = millis();
  drawScreen("Testing...");
  Serial.println(F("Ballpen test command sent to Uno."));
}

void stopBallpenTest() {
  Serial3.println(F("BALLPEN_STOP"));
  waitingForUno = false;
  drawScreen("Stopped");
  Serial.println(F("Ballpen stop command sent to Uno."));
}

void handleUnoMessages() {
  while (Serial3.available() > 0) {
    String message = Serial3.readStringUntil('\n');
    message.trim();

    if (message == "BALLPEN_RESULT:DETECTED") {
      waitingForUno = false;
      drawScreen("Detected");
      Serial.println(F("Ballpen Uno: IR detected."));
    } else if (message == "BALLPEN_RESULT:NOTHING") {
      waitingForUno = false;
      drawScreen("Nothing");
      Serial.println(F("Ballpen Uno: no IR detection."));
    } else if (message == "BALLPEN_STOPPED") {
      waitingForUno = false;
      drawScreen("Stopped");
      Serial.println(F("Ballpen Uno: motor stopped."));
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

  if (x >= TEST_BUTTON_X && x <= TEST_BUTTON_X + TEST_BUTTON_W &&
      y >= TEST_BUTTON_Y && y <= TEST_BUTTON_Y + TEST_BUTTON_H) {
    startBallpenTest();
  } else if (x >= STOP_BUTTON_X && x <= STOP_BUTTON_X + STOP_BUTTON_W &&
             y >= STOP_BUTTON_Y && y <= STOP_BUTTON_Y + STOP_BUTTON_H) {
    stopBallpenTest();
  }
}

void setup() {
  Serial.begin(115200);
  Serial3.begin(UART_BAUD_RATE);

  tft.begin();
  tft.setRotation(2);
  touchscreen.begin();
  touchscreen.setRotation(0);
  drawScreen("Ballpen Uno Ready");

  Serial.println(F("Mega ballpen-controller test ready."));
  Serial.println(F("Use TEST BALLPEN or STOP on the TFT."));
}

void loop() {
  handleUnoMessages();
  handleTouch();

  if (waitingForUno && millis() - responseWaitStartedAt >= RESPONSE_TIMEOUT_MS) {
    waitingForUno = false;
    drawScreen("Uno timeout");
    Serial.println(F("No response received from ballpen Uno."));
  }
}

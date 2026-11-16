/*
  Standalone ILI9341 + XPT2046 diagnostic for Arduino Mega 2560.
  TFT and touch share SPI signal lines, but each signal stays separate.

  Mega D50 MISO <- TFT SDO and touch T_DO
  Mega D51 MOSI -> TFT SDI and touch T_DIN
  Mega D52 SCK  -> TFT SCK and touch T_CLK
  Mega D53      -> TFT CS
  Mega D48      -> TFT DC/RS
  Mega D49      -> TFT RST
  Mega D47      -> touch T_CS
  Mega GND      -> module GND

  Connect VCC and LED/BL only according to the module's voltage markings.
  A bare 3.3 V module needs compatible 3.3 V logic/level shifting with a Mega.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS 53
#define TFT_DC 48
#define TFT_RST 49
#define TOUCH_CS 47

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);

void drawTestPattern() {
  tft.fillScreen(ILI9341_BLACK);
  tft.fillRect(0, 0, 120, 90, ILI9341_RED);
  tft.fillRect(120, 0, 120, 90, ILI9341_GREEN);
  tft.fillRect(0, 90, 120, 90, ILI9341_BLUE);
  tft.fillRect(120, 90, 120, 90, ILI9341_YELLOW);
  tft.fillRect(0, 180, 120, 90, ILI9341_CYAN);
  tft.fillRect(120, 180, 120, 90, ILI9341_MAGENTA);
  tft.drawRect(2, 2, 236, 316, ILI9341_WHITE);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 292);
  tft.print(F("TFT + TOUCH TEST"));
}

void setup() {
  Serial.begin(115200);
  pinMode(TFT_CS, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  digitalWrite(TOUCH_CS, HIGH);

  Serial.println(F("Starting ILI9341/XPT2046 test at 115200 baud"));
  tft.begin();
  tft.setRotation(2);
  drawTestPattern();
  Serial.println(F("TFT init called; color pattern should be visible."));

  bool touchOk = touch.begin();
  touch.setRotation(0);
  Serial.print(F("XPT2046 begin: "));
  Serial.println(touchOk ? F("OK") : F("FAILED"));
  Serial.println(F("Touch the display; raw X/Y/Z readings will print here."));
}

void loop() {
  if (!touch.touched()) return;

  TS_Point p = touch.getPoint();
  Serial.print(F("Touch raw: X="));
  Serial.print(p.x);
  Serial.print(F(" Y="));
  Serial.print(p.y);
  Serial.print(F(" Z="));
  Serial.println(p.z);
  delay(120);
}

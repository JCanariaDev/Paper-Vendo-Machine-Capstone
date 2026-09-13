/*
  Arduino Uno ILI9341 + XPT2046 TFT touch/color test

  Libraries (Arduino Library Manager):
    - Adafruit GFX Library
    - Adafruit ILI9341
    - Adafruit SH110X
    - XPT2046_Touchscreen

  UNO <-> SPI TFT / XPT2046 wiring
    D13 -> SCK / CLK       D12 <- MISO / SDO
    D11 -> MOSI / SDI      D10 -> TFT_CS
    D9  -> TFT_DC          D8  -> TFT_RST
    D7  -> T_CS            GND -> GND

  I2C SH1106 OLED wiring (128x64, usually address 0x3C)
    UNO A4 -> SDA          UNO A5 -> SCL
    GND -> GND             VCC -> supply voltage marked on the OLED

  Power the display according to its label.  A bare 3.3 V TFT must use 3.3 V
  power and 3.3 V logic; a module marked 5V/3.3V normally has a regulator and
  level shifting. Do not connect a 3.3 V-only module directly to UNO 5 V pins.
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_SH110X.h>
#include <XPT2046_Touchscreen.h>

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
#define TOUCH_CS   7

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);
Adafruit_SH1106G oled(128, 64, &Wire, -1);
bool oledOk = false;

// Calibration copied from the Mega TFT setup. Adjust these if Serial Monitor
// shows touches that do not correspond to the spot pressed.
const int TS_MINX = 328, TS_MAXX = 3531;
const int TS_MINY = 336, TS_MAXY = 3434;

struct ColorButton {
  uint16_t color;
  const char *label;
};

const ColorButton buttons[] = {
  {ILI9341_RED,     "RED"},
  {ILI9341_GREEN,   "GREEN"},
  {ILI9341_BLUE,    "BLUE"},
  {ILI9341_YELLOW,  "YELLOW"},
  {ILI9341_MAGENTA, "MAGENTA"},
  {ILI9341_CYAN,    "CYAN"}
};
const uint8_t BUTTON_COUNT = sizeof(buttons) / sizeof(buttons[0]);

void drawOledStatus(const char *selected, int x, int y) {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println(F("UNO OLED TEST"));
  oled.drawFastHLine(0, 11, 128, SH110X_WHITE);
  oled.setCursor(0, 19);
  oled.print(F("Color: "));
  oled.println(selected);
  oled.setCursor(0, 32);
  if (x >= 0) {
    oled.print(F("Touch X: "));
    oled.println(x);
    oled.print(F("Touch Y: "));
    oled.println(y);
  } else {
    oled.println(F("Touch TFT buttons"));
    oled.println(F("to test OLED too."));
  }
  oled.display();
}

void drawButton(uint8_t index, int x, int y, int w, int h) {
  uint16_t textColor = (buttons[index].color == ILI9341_BLUE ||
                        buttons[index].color == ILI9341_RED ||
                        buttons[index].color == ILI9341_MAGENTA) ? ILI9341_WHITE : ILI9341_BLACK;
  tft.fillRoundRect(x, y, w, h, 6, buttons[index].color);
  tft.drawRoundRect(x, y, w, h, 6, ILI9341_WHITE);
  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.setCursor(x + 8, y + 16);
  tft.print(buttons[index].label);
}

void drawScreen(uint16_t background, const char *message) {
  tft.fillScreen(background);
  tft.fillRect(0, 0, tft.width(), 42, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 7);
  tft.print("UNO TFT TOUCH TEST");
  tft.setTextSize(1);
  tft.setCursor(12, 28);
  tft.print(message);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;
    drawButton(i, 12 + col * 115, 62 + row * 70, 100, 52);
  }
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(2);       // landscape; change to 0 if your display is upright
  touch.begin();
  touch.setRotation(0);
  oledOk = oled.begin(0x3C, true);
  if (!oledOk) {
    Serial.println(F("SH1106 OLED not found at I2C address 0x3C."));
  }
  drawScreen(ILI9341_BLACK, "Touch a colored button");
  drawOledStatus("NONE", -1, -1);
  Serial.println(F("TFT touch/color test ready."));
}

void loop() {
  if (!touch.touched()) return;

  TS_Point p = touch.getPoint();
  int x = constrain(map(p.x, TS_MINX, TS_MAXX, 0, tft.width() - 1), 0, tft.width() - 1);
  int y = constrain(map(p.y, TS_MINY, TS_MAXY, 0, tft.height() - 1), 0, tft.height() - 1);
  Serial.print(F("raw x=")); Serial.print(p.x);
  Serial.print(F(" raw y=")); Serial.print(p.y);
  Serial.print(F("  screen x=")); Serial.print(x);
  Serial.print(F(" y=")); Serial.println(y);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;
    int bx = 12 + col * 115;
    int by = 62 + row * 70;
    if (x >= bx && x < bx + 100 && y >= by && y < by + 52) {
      drawScreen(buttons[i].color, buttons[i].label);
      drawOledStatus(buttons[i].label, x, y);
      delay(250); // simple debounce
      break;
    }
  }

  while (touch.touched()) delay(10); // one action per press
}

/*
  Arduino Mega 2560 TFT + Touch + OLED test
  Uses the SAME display pins and touch calibration as revamped_final_mega.

  Required libraries (Arduino Library Manager):
    Adafruit GFX Library, Adafruit ILI9341, XPT2046_Touchscreen, Adafruit SH110X

  ILI9341 TFT + XPT2046 touch (shared Mega hardware SPI)
    Mega D50  MISO  <- TFT SDO / touch T_DO
    Mega D51  MOSI  -> TFT SDI / touch T_DIN
    Mega D52  SCK   -> TFT SCK / touch T_CLK
    Mega D53         -> TFT CS
    Mega D48         -> TFT DC
    Mega D49         -> TFT RST
    Mega D47         -> Touch T_CS
    Mega GND         -> TFT/touch GND

  SH1106 128x64 OLED (I2C, normally address 0x3C)
    Mega D20 / SDA -> OLED SDA
    Mega D21 / SCL -> OLED SCL
    Mega GND       -> OLED GND

  Use the voltage marked on each display module. A bare 3.3 V TFT must not
  receive 5 V power or direct 5 V Mega logic without level shifting.
*/

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_SH110X.h>
#include <XPT2046_Touchscreen.h>

// Exact production Mega pin assignments.
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen touch(TOUCH_CS);
Adafruit_SH1106G oled(128, 64, &Wire, -1);
bool oledOk = false;

// Exact calibration from revamped_final_mega.ino.
#define TS_MINX 328
#define TS_MAXX 3531
#define TS_MINY 336
#define TS_MAXY 3434

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

void drawOledStatus(const char *colorName, int x, int y) {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.setTextColor(SH110X_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.println(F("MEGA DISPLAY TEST"));
  oled.drawFastHLine(0, 11, 128, SH110X_WHITE);
  oled.setCursor(0, 18);
  oled.print(F("Color: "));
  oled.println(colorName);
  oled.setCursor(0, 32);
  if (x >= 0) {
    oled.print(F("Touch X: ")); oled.println(x);
    oled.print(F("Touch Y: ")); oled.println(y);
  } else {
    oled.println(F("Touch a TFT color"));
    oled.println(F("button to test."));
  }
  oled.display();
}

void drawButton(uint8_t index, int x, int y) {
  const int w = 104, h = 48;
  uint16_t textColor = (buttons[index].color == ILI9341_BLUE ||
                        buttons[index].color == ILI9341_RED ||
                        buttons[index].color == ILI9341_MAGENTA) ? ILI9341_WHITE : ILI9341_BLACK;
  tft.fillRoundRect(x, y, w, h, 6, buttons[index].color);
  tft.drawRoundRect(x, y, w, h, 6, ILI9341_WHITE);
  tft.setTextColor(textColor);
  tft.setTextSize(2);
  tft.setCursor(x + 9, y + 16);
  tft.print(buttons[index].label);
}

void drawTftScreen(uint16_t background, const char *message) {
  tft.fillScreen(background);
  tft.fillRect(0, 0, tft.width(), 42, ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(8, 7);
  tft.print(F("MEGA TFT TEST"));
  tft.setTextSize(1);
  tft.setCursor(8, 29);
  tft.print(message);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    int col = i % 2;
    int row = i / 2;
    drawButton(i, 8 + col * 120, 58 + row * 65);
  }
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(2);  // Exact orientation in the main Mega sketch.
  touch.begin();
  touch.setRotation(0);
  oledOk = oled.begin(0x3C, true);

  drawTftScreen(ILI9341_BLACK, "Touch a colored button");
  drawOledStatus("NONE", -1, -1);
  Serial.println(F("Mega TFT/touch/OLED test ready at 115200 baud."));
  if (!oledOk) Serial.println(F("OLED not found at address 0x3C."));
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
    int bx = 8 + col * 120;
    int by = 58 + row * 65;
    if (x >= bx && x < bx + 104 && y >= by && y < by + 48) {
      drawTftScreen(buttons[i].color, buttons[i].label);
      drawOledStatus(buttons[i].label, x, y);
      delay(250); // debounce
      break;
    }
  }

  while (touch.touched()) delay(10); // one action per press
}

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Servo.h>
#include <Stepper.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <avr/wdt.h>   // hardware watchdog — used for hardware reset button

/*
  ==============================================================================
  REVAMPED ARDUINO MEGA 2560 — MASTER CONTROLLER (OPTION A DUAL-BOARD SYSTEM)
  - Manages ILI9341 Touch UI, SH1106 OLED, Coin Acceptor, Coin Hopper, and 3 Pens.
  - Communicates with ESP32 (Cloud Gateway) via Serial1 (Pins 18/19).
  - Communicates with Arduino Uno (Dedicated 4-Bay Paper Controller) via Serial2 (Pins 16/17).
  ==============================================================================
*/

// =============================================================
// ARDUINO MEGA 2560 — PIN ASSIGNMENTS (OPTION A ARCHITECTURE)
// =============================================================
//
// ── DIGITAL I/O ──────────────────────────────────────────────
//  D2   COIN_PIN            Coin acceptor pulse input (INPUT_PULLUP, INT0)
//  D3   penStepper1 IN1     28BYJ-48 pen slot 1, ULN2003 coil A
//  D4   penStepper1 IN2     28BYJ-48 pen slot 1, ULN2003 coil B
//  D6   COIN_INHIBIT_PIN    Coin acceptor INHIBIT line (OUTPUT, active HIGH)
//  D7   PEN_IR_PIN          IR sensor pen slot 1 (INPUT_PULLUP, LOW = beam broken)
//  D8   LED_GREEN_PIN       Green LED — machine READY / AVAILABLE
//  D9   SERVO_CHANGE_PIN    Change-dispense servo signal
//  D10  SERVO_PEN_PIN       Pen-ejection servo signal
//  D11  penStepper1 IN3     28BYJ-48 pen slot 1, ULN2003 coil C
//  D12  penStepper1 IN4     28BYJ-48 pen slot 1, ULN2003 coil D
//  D13  LED_BLUE_PIN        Blue LED  — machine IDLE / IN USE (busy)
//  D14  CHANGE_HOPPER_MOTOR_PIN  Relay IN controlling coin hopper motor
//  D15  CHANGE_HOPPER_SENSOR_PIN Coin hopper exit IR sensor (INPUT_PULLUP)
//  D16  TX2 (Serial2)       → Arduino Uno RX (Pin D0) at 5V logic
//  D17  RX2 (Serial2)       ← Arduino Uno TX (Pin D1) at 5V logic
//  D18  TX1 (Serial1)       → ESP32 RX2 (via 3.3V logic level converter)
//  D19  RX1 (Serial1)       ← ESP32 TX2 (via 3.3V logic level converter)
//  D22  penStepper2 IN1     28BYJ-48 pen slot 2, ULN2003 coil A
//  D23  penStepper2 IN2     28BYJ-48 pen slot 2, ULN2003 coil B
//  D24  penStepper2 IN3     28BYJ-48 pen slot 2, ULN2003 coil C
//  D25  penStepper2 IN4     28BYJ-48 pen slot 2, ULN2003 coil D
//  D26  penStepper3 IN1     28BYJ-48 pen slot 3, ULN2003 coil A
//  D27  penStepper3 IN2     28BYJ-48 pen slot 3, ULN2003 coil B
//  D28  penStepper3 IN3     28BYJ-48 pen slot 3, ULN2003 coil C
//  D29  penStepper3 IN4     28BYJ-48 pen slot 3, ULN2003 coil D
//  D30  PEN_IR_PIN2         IR sensor pen slot 2 (INPUT_PULLUP)
//  D31  PEN_IR_PIN3         IR sensor pen slot 3 (INPUT_PULLUP)
//  D45  LED_RED_PIN         Red LED   — machine ERROR state
//  D46  BUZZER_PIN          Passive buzzer (2-pin, driven by tone())
//  D47  TOUCH_CS            XPT2046 touchscreen chip select (SPI)
//  D48  TFT_DC              ILI9341 TFT data/command
//  D49  TFT_RST             ILI9341 TFT reset
//  D50  MISO  (hardware SPI) shared TFT + touch
//  D51  MOSI  (hardware SPI) shared TFT + touch
//  D52  SCK   (hardware SPI) shared TFT + touch
//  D53  TFT_CS              ILI9341 TFT chip select (SPI)
//
// ── ANALOG HEADER (Used as Digital Inputs) ────────────────────
//  A8   HW_RESET_BTN_PIN    Hardware-reset push button (INPUT_PULLUP)
//  A9   SW_RESET_BTN_PIN    Software-reset push button (INPUT_PULLUP)
//
// ── I²C BUS (Wire) ───────────────────────────────────────────
//  D20  SDA    SH1106G OLED 128×64 (address 0x3C)
//  D21  SCL    SH1106G OLED 128×64
// =============================================================

// --- PINS (existing) ---
const int COIN_PIN = 2;
const int COIN_INHIBIT_PIN = 6; // Moved from D16 to D6 to free D16/D17 for Serial2 (Uno)
const bool COIN_INHIBIT_ACTIVE_HIGH = true;

const int LED_GREEN_PIN = 8;
const int LED_BLUE_PIN = 13;
const int LED_RED_PIN = 45;
const int BUZZER_PIN = 46;

const int HW_RESET_BTN_PIN = A8;
const int SW_RESET_BTN_PIN = A9;
const int PEN_IR_PIN = 7;
const int PEN_IR_PIN2 = 30;
const int PEN_IR_PIN3 = 31;
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;

const int CHANGE_HOPPER_MOTOR_PIN  = 14;
const int CHANGE_HOPPER_SENSOR_PIN = 15;
const unsigned long CHANGE_COIN_TIMEOUT_MS  = 5000;
const unsigned long PEN_SENSOR_TIMEOUT_MS   = 5000;
const unsigned long HOPPER_MANUAL_MAX_MS    = 10000;
const unsigned long PAPER_DISPENSE_TIMEOUT_MS = 15000;

const int HOPPER_RELAY_ON  = HIGH;
const int HOPPER_RELAY_OFF = (HOPPER_RELAY_ON == HIGH) ? LOW : HIGH;

// --- TFT & TOUCH PINS ---
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

// --- PEN STEPPERS ---
const int stepsPerRevolution = 2048;
Stepper penStepper1(stepsPerRevolution, 3, 11, 4, 12);
Stepper penStepper2(stepsPerRevolution, 22, 24, 23, 25);
Stepper penStepper3(stepsPerRevolution, 26, 28, 27, 29);
Stepper* penSteppers[3] = { &penStepper1, &penStepper2, &penStepper3 };
const int penStopPins[3][4] = {
  { 3, 4, 11, 12 },
  { 22, 23, 24, 25 },
  { 26, 27, 28, 29 }
};
const int penIrPins[3] = { PEN_IR_PIN, PEN_IR_PIN2, PEN_IR_PIN3 };

// --- PERIPHERALS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define CLOUD_SERIAL Serial1 // ESP32 Gateway (Pins 18/19)
#define UNO_SERIAL   Serial2 // Uno Paper Controller (Pins 16/17)

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo servoChange, servoPen;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);

// --- TOUCH CALIBRATION ---
#define TS_MINX 328
#define TS_MAXX 3531
#define TS_MINY 336
#define TS_MAXY 3434

#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

// RGB565 Colors
#define COL_BLACK     0x0000
#define COL_WHITE     0xFFFF
#define COL_RED       0xF800
#define COL_GREEN     0x07E0
#define COL_BLUE      0x001F
#define COL_ORANGE    0xFD20
#define COL_DARKGREEN 0x03E0
#define COL_GREY      0x39C7

// --- STATE ---
volatile uint16_t credits = 0;
volatile bool coinPulseReceived = false;
bool isProcessing = false;
String activeTransactionId = "";
int activeChangeCents = 0;
String selectedPaperBrand = "Budget";

// --- DIAGNOSTICS STATE ---
bool diagOledOk = false;
bool diagTftOk = false;
bool diagTouchOk = false;
bool hopperManualRunning = false;
unsigned long hopperManualStartedAt = 0;

enum IndicatorState { INDICATOR_READY, INDICATOR_ACTIVE, INDICATOR_ERROR };
IndicatorState indicatorState = INDICATOR_READY;

unsigned long hwResetDebounceUntil = 0;
unsigned long swResetDebounceUntil = 0;

// Forward declarations
void setMachineIndicator(IndicatorState state, bool sound = false);
void printCentered(const char* text, int cx, int cy);
float cartTotal();
float catalogDisplayPrice(int index);
void setCoinAcceptance(bool allowed);
void drawTftStatusBar();
void resetPendingSelections();
void drawIdleScreen();
void drawMainScreen();
void drawPaperBrandScreen();
void drawCatalogScreen();
void drawSummaryScreen();
void drawCartScreen();
void redrawCurrentScreen();
void handleMainTouch(int x, int y);
void handlePaperBrandTouch(int x, int y);
void handleCatalogTouch(int x, int y);
void handleCartTouch(int x, int y);
void tftUiShowError(String message);
void tftUiShowSuccess(String message);
void updateLCD();
void showError(String m);
void drawStatusScreen(String headline, String message);
void refreshMachineAvailability(bool sound = false);
void stopStepper(int penIndex);
bool dispenseOnePen(int channel);
int dispensePaperFromUno(int bayNumber, int sheetCount);
int releaseVerifiedChange(int changeCents);
void handleCloudCommand(String msg);
void handleUnoMessage(String msg);
void coinInterrupt();
void softResetMachineState();

void setMachineIndicator(IndicatorState state, bool sound) {
  indicatorState = state;
  digitalWrite(LED_GREEN_PIN, state == INDICATOR_READY  ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN,  state == INDICATOR_ACTIVE ? HIGH : LOW);
  digitalWrite(LED_RED_PIN,   state == INDICATOR_ERROR  ? HIGH : LOW);

  if (!sound) return;
  switch (state) {
    case INDICATOR_READY:
      tone(BUZZER_PIN, 1800, 80);
      delay(140);
      tone(BUZZER_PIN, 1800, 80);
      break;
    case INDICATOR_ACTIVE:
      tone(BUZZER_PIN, 1100, 120);
      break;
    case INDICATOR_ERROR:
      tone(BUZZER_PIN, 350, 500);
      break;
  }
}

// ================= CATALOG =================
struct CatalogItem {
  int id;
  const char* name;
  float price;
  bool isPaperPresent; // Synced from Uno's L5290 sensors
};

const int PAPER_COUNT = 4;
CatalogItem paperCatalog[PAPER_COUNT] = {
  {1, "Bay 1 Paper",  1.00, true},
  {2, "Bay 2 Paper",  1.00, true},
  {3, "Bay 3 Paper",  1.00, true},
  {4, "Bay 4 Paper",  1.00, true}
};

const int BALLPEN_COUNT = 3;
CatalogItem ballpenCatalog[BALLPEN_COUNT] = {
  {1, "Pen Slot 1", 5.00, true},
  {2, "Pen Slot 2", 5.00, true},
  {3, "Pen Slot 3", 5.00, true}
};

// Dynamic name buffers — updated by ESP32 PAPER_BAY: / PEN_BAY: messages
char paperCatalogNames[PAPER_COUNT][32];
char ballpenCatalogNames[BALLPEN_COUNT][32];
int  paperCatalogStock[BALLPEN_COUNT]; // pen stock per bay (pieces)

const int MAX_CATALOG_ROWS = 4;
int pendingQty[MAX_CATALOG_ROWS];
String activeCatalogType = "paper";

// ================= CART =================
struct CartItem {
  String type;
  int id;
  String name;
  float price;
  int qty;
};
const int MAX_CART_ITEMS = 8;
CartItem cart[MAX_CART_ITEMS];
int cartCount = 0;

// ================= UI STATE =================
enum UiScreen { SCREEN_IDLE, SCREEN_MAIN, SCREEN_PAPER_BRAND, SCREEN_CATALOG, SCREEN_CART, SCREEN_SUMMARY };
UiScreen currentScreen = SCREEN_IDLE;

bool uiWifiConnected = false;
enum WifiStatus { WIFI_STATUS_IDLE, WIFI_STATUS_CONNECTING, WIFI_STATUS_NOT_FOUND, WIFI_STATUS_CONNECTED };
WifiStatus wifiStatus = WIFI_STATUS_IDLE;
int spinnerFrame = 0;
unsigned long lastSpinnerUpdate = 0;
const char SPINNER_CHARS[4] = { '|', '/', '-', '\\' };
unsigned long uiErrorUntil = 0;
unsigned long uiSuccessUntil = 0;
unsigned long touchDebounceUntil = 0;

volatile bool orderInProgress = false;
int cartDispenseIndex = 0;
String orderSummaryText = "";
float orderTotalCost = 0;

void refreshMachineAvailability(bool sound) {
  if (orderInProgress || wifiStatus == WIFI_STATUS_CONNECTING || wifiStatus == WIFI_STATUS_IDLE) {
    setMachineIndicator(INDICATOR_ACTIVE, sound);
  } else if (wifiStatus == WIFI_STATUS_CONNECTED) {
    setMachineIndicator(INDICATOR_READY, sound);
  } else {
    setMachineIndicator(INDICATOR_ERROR, sound);
  }
}

// ================= DRAWING HELPERS =================
void printCentered(const char* text, int cx, int cy) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - h / 2);
  tft.print(text);
}

float cartTotal() {
  float sum = 0;
  for (int i = 0; i < cartCount; i++) sum += cart[i].price * cart[i].qty;
  return sum;
}

float catalogDisplayPrice(int index) {
  // Price is dynamically set from ESP32 catalog sync per bay
  if (activeCatalogType == "paper") {
    return paperCatalog[index].price;
  }
  return ballpenCatalog[index].price;
}

void setCoinAcceptance(bool allowed) {
  int level = allowed
    ? (COIN_INHIBIT_ACTIVE_HIGH ? LOW : HIGH)
    : (COIN_INHIBIT_ACTIVE_HIGH ? HIGH : LOW);
  digitalWrite(COIN_INHIBIT_PIN, level);
}

const int CATALOG_TOP = 58;
const int CATALOG_BOTTOM = 268;

int catalogRowHeight(int count) {
  return (CATALOG_BOTTOM - CATALOG_TOP) / count;
}

int catalogRowY(int i, int count) {
  return CATALOG_TOP + i * catalogRowHeight(count);
}

int catalogButtonSize(int count) {
  return catalogRowHeight(count) - 8;
}

void drawTftStatusBar() {
  tft.fillRect(0, 0, tft.width(), 26, COL_BLACK);
  tft.setTextSize(1);

  tft.setTextColor(uiWifiConnected ? COL_GREEN : COL_RED);
  tft.setCursor(6, 8);
  tft.print(uiWifiConnected ? "WIFI OK" : "WIFI --");

  String creditText = "Credits: P" + String((unsigned int)credits);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(creditText.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(tft.width() - w - 6, 8);
  tft.print(creditText);
}

void resetPendingSelections() {
  for (int i = 0; i < MAX_CATALOG_ROWS; i++) {
    pendingQty[i] = 0;
  }
}

void drawIdleScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  if (wifiStatus == WIFI_STATUS_CONNECTED) {
    printCentered("Insert Coins", tft.width() / 2, 140);
    printCentered("to use.", tft.width() / 2, 165);
    return;
  }

  printCentered("Connect to WiFi", tft.width() / 2, 140);
  printCentered("first.", tft.width() / 2, 165);

  tft.setTextSize(1);
  if (wifiStatus == WIFI_STATUS_NOT_FOUND) {
    tft.setTextColor(COL_RED);
    printCentered("WiFi can't be detected", tft.width() / 2, 200);
  }
}

void drawMainScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Select an Option", tft.width() / 2, 48);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  printCentered("BUY PAPER", tft.width() / 2, 122);

  tft.fillRoundRect(20, 160, 200, 55, 8, COL_GREEN);
  printCentered("BUY BALLPEN", tft.width() / 2, 187);

  tft.fillRoundRect(20, 225, 200, 55, 8, COL_DARKGREEN);
  printCentered("CHECKOUT", tft.width() / 2, 252);

  tft.fillRoundRect(20, 285, 200, 30, 8, COL_GREY);
  tft.setTextSize(1);
  String cartLabel = cartCount > 0 ? ("VIEW CART (" + String(cartCount) + ")") : "VIEW CART (empty)";
  printCentered(cartLabel.c_str(), tft.width() / 2, 285 + 15);
}

void drawPaperBrandScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Choose Paper Brand", tft.width() / 2, 55);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  printCentered("BUDGET", tft.width() / 2, 122);
  tft.fillRoundRect(20, 165, 200, 55, 8, COL_GREEN);
  printCentered("STANDARD", tft.width() / 2, 192);
  tft.fillRoundRect(20, 275, 200, 35, 8, COL_GREY);
  tft.setTextSize(1);
  printCentered("BACK", tft.width() / 2, 292);
}

void drawCatalogScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  String title = activeCatalogType == "paper" ? selectedPaperBrand + " Paper" : "Ballpen Options";
  printCentered(title.c_str(), tft.width() / 2, 38);

  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int btnSize = catalogButtonSize(count);
    bool selected = pendingQty[i] > 0;
    bool isAvailable = catalog[i].isPaperPresent;

    if (selected) {
      tft.drawRoundRect(4, rowY, 232, btnSize + 8, 8, COL_GREEN);
      tft.drawRoundRect(5, rowY + 1, 230, btnSize + 6, 8, COL_GREEN);
    }

    // "-" button
    tft.fillRoundRect(8, rowY + 4, btnSize, btnSize, 8, COL_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(btnSize >= 46 ? 3 : 2);
    printCentered("-", 8 + btnSize / 2, rowY + 4 + btnSize / 2);

    // "+" button
    int plusX = 232 - btnSize;
    tft.fillRoundRect(plusX, rowY + 4, btnSize, btnSize, 8, isAvailable ? COL_GREEN : COL_GREY);
    printCentered("+", plusX + btnSize / 2, rowY + 4 + btnSize / 2);

    // Info Column
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(1);
    tft.setCursor(72, rowY + 6);
    tft.print(catalog[i].name);
    tft.setCursor(72, rowY + 20);
    tft.print("P" + String(catalogDisplayPrice(i), 2));

    if (!isAvailable) {
      tft.setTextColor(COL_RED);
      tft.setCursor(72, rowY + 34);
      tft.print("[OUT OF PAPER]");
    } else {
      char qtyBuf[16];
      sprintf(qtyBuf, "Qty: %d", pendingQty[i]);
      tft.setTextSize(btnSize >= 46 ? 2 : 1);
      tft.setCursor(72, rowY + (btnSize >= 46 ? 34 : 32));
      tft.print(qtyBuf);
    }
  }

  tft.fillRoundRect(15, 275, 95, 35, 8, COL_GREEN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("ADD", 15 + 47, 275 + 17);

  tft.fillRoundRect(130, 275, 95, 35, 8, COL_RED);
  printCentered("CANCEL", 130 + 47, 275 + 17);
}

void drawSummaryScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Order Summary", tft.width() / 2, 40);

  tft.setTextSize(1);
  int y = 70;
  int start = 0;
  while (start < (int)orderSummaryText.length()) {
    int nl = orderSummaryText.indexOf('\n', start);
    String line = (nl == -1) ? orderSummaryText.substring(start) : orderSummaryText.substring(start, nl);
    if (line.length() > 0) {
      tft.setCursor(15, y);
      tft.print(line);
      y += 16;
    }
    if (nl == -1) break;
    start = nl + 1;
  }

  tft.setCursor(15, y + 10);
  tft.print("Total: P" + String(orderTotalCost, 2));

  tft.setTextSize(2);
  if (orderInProgress) {
    tft.setTextColor(COL_ORANGE);
    printCentered("Dispensing...", tft.width() / 2, 258);
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_GREY);
    tft.setTextColor(COL_WHITE);
    printCentered("PLEASE WAIT", tft.width() / 2, 275 + 20);
  } else {
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_GREY);
    tft.setTextColor(COL_WHITE);
    printCentered("ORDER CLOSED", tft.width() / 2, 275 + 20);
  }
}

const int CART_TOP = 58;
const int CART_BOTTOM = 248;

int cartRowHeight() {
  if (cartCount <= 0) return CART_BOTTOM - CART_TOP;
  return (CART_BOTTOM - CART_TOP) / cartCount;
}

int cartRowY(int i) {
  return CART_TOP + i * cartRowHeight();
}

void removeFromCart(int index) {
  if (index < 0 || index >= cartCount) return;
  for (int i = index; i < cartCount - 1; i++) {
    cart[i] = cart[i + 1];
  }
  cartCount--;
}

void drawCartScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Your Cart", tft.width() / 2, 45);

  tft.setTextSize(1);
  if (cartCount == 0) {
    printCentered("Cart is empty.", tft.width() / 2, 150);
  } else {
    int rowH = cartRowHeight();
    for (int i = 0; i < cartCount; i++) {
      int rowY = cartRowY(i);

      String line = String(cart[i].qty) + "x " + cart[i].name;
      tft.setCursor(10, rowY + 4);
      tft.print(line);
      String priceLine = "P" + String(cart[i].price, 2) + " each - P" + String(cart[i].price * cart[i].qty, 2);
      tft.setCursor(10, rowY + 18);
      tft.print(priceLine);

      int btnSize = min(rowH - 6, 40);
      int btnY = rowY + (rowH - btnSize) / 2;
      tft.fillRoundRect(196, btnY, btnSize, btnSize, 6, COL_RED);
      tft.setTextColor(COL_WHITE);
      tft.setTextSize(btnSize >= 24 ? 2 : 1);
      printCentered("X", 196 + btnSize / 2, btnY + btnSize / 2);
      tft.setTextSize(1);

      if (i < cartCount - 1) {
        tft.drawFastHLine(8, rowY + rowH - 2, 224, COL_GREY);
      }
    }

    tft.setCursor(10, CART_BOTTOM + 6);
    tft.setTextSize(2);
    tft.print("Total: P" + String(cartTotal(), 2));
  }

  tft.fillRoundRect(20, 275, 200, 35, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("BACK", tft.width() / 2, 275 + 17);
}

void redrawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_IDLE:        drawIdleScreen();        break;
    case SCREEN_MAIN:        drawMainScreen();        break;
    case SCREEN_PAPER_BRAND: drawPaperBrandScreen();   break;
    case SCREEN_CATALOG:     drawCatalogScreen();     break;
    case SCREEN_CART:        drawCartScreen();        break;
    case SCREEN_SUMMARY:     drawSummaryScreen();     break;
  }
}

void addToCart(String type, int id, const char* name, float price, int qty) {
  for (int i = 0; i < cartCount; i++) {
    if (cart[i].type == type && cart[i].id == id) {
      cart[i].qty += qty;
      return;
    }
  }
  if (cartCount < MAX_CART_ITEMS) {
    cart[cartCount].type = type;
    cart[cartCount].id = id;
    cart[cartCount].name = String(name);
    cart[cartCount].price = price;
    cart[cartCount].qty = qty;
    cartCount++;
  }
}

void startOrder() {
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  orderInProgress = true;
  setMachineIndicator(INDICATOR_ACTIVE, true);
  setCoinAcceptance(false);
  orderSummaryText = "";
  orderTotalCost = 0;
  activeTransactionId = "";
  activeChangeCents = 0;
  currentScreen = SCREEN_SUMMARY;
  drawSummaryScreen();

  String encodedLines = "";
  for (int i = 0; i < cartCount; i++) {
    if (encodedLines.length()) encodedLines += ';';
    encodedLines += cart[i].type + "," + String(cart[i].id) + "," + String(cart[i].qty);
  }
  CLOUD_SERIAL.println("RESERVE:" + String((unsigned long)credits * 100UL) + ":" + encodedLines);
}

void handleMainTouch(int x, int y) {
  // VIEW CART — works even offline
  if (x >= 20 && x <= 230 && y >= 285 && y <= 315) {
    currentScreen = SCREEN_CART;
    drawCartScreen();
    return;
  }

  if (!uiWifiConnected) {
    tftUiShowError("Connect to WiFi first");
    return;
  }

  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    // BUY PAPER: go directly to 4-bay catalog (no brand picker)
    activeCatalogType = "paper";
    resetPendingSelections();
    currentScreen = SCREEN_CATALOG;
    drawCatalogScreen();
  } else if (x >= 20 && x <= 220 && y >= 160 && y <= 215) {
    activeCatalogType = "pen";
    resetPendingSelections();
    currentScreen = SCREEN_CATALOG;
    drawCatalogScreen();
  } else if (x >= 20 && x <= 220 && y >= 225 && y <= 280) {
    if (cartCount > 0) {
      startOrder();
    } else {
      tftUiShowError("Cart is empty");
    }
  }
}

void handlePaperBrandTouch(int x, int y) {
  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    selectedPaperBrand = "Budget";
  } else if (x >= 20 && x <= 220 && y >= 165 && y <= 220) {
    selectedPaperBrand = "Standard";
  } else if (x >= 20 && x <= 220 && y >= 275 && y <= 315) {
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  } else {
    return;
  }
  resetPendingSelections();
  currentScreen = SCREEN_CATALOG;
  drawCatalogScreen();
}

void handleCatalogTouch(int x, int y) {
  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int btnSize = catalogButtonSize(count);
    int plusX = 232 - btnSize;

    if (x >= 4 && x <= 8 + btnSize + 4 && y >= rowY && y <= rowY + btnSize + 8) {
      pendingQty[i] = max(0, pendingQty[i] - 1);
      drawCatalogScreen();
      return;
    }
    if (x >= plusX - 4 && x <= plusX + btnSize + 4 && y >= rowY && y <= rowY + btnSize + 8) {
      if (!catalog[i].isPaperPresent) {
        tftUiShowError(activeCatalogType == "paper" ? "Bay is Out of Paper" : "Out of Stock");
        return;
      }

      float pendingCost = 0;
      for (int j = 0; j < count; j++) pendingCost += pendingQty[j] * catalogDisplayPrice(j);
      float wouldBeCost = cartTotal() + pendingCost + catalogDisplayPrice(i);

      if (wouldBeCost > credits) {
        tftUiShowError("Insufficient credits");
        return;
      }

      pendingQty[i] = min(20, pendingQty[i] + 1);
      drawCatalogScreen();
      return;
    }
  }

  if (x >= 20 && x <= 120 && y >= 270 && y <= 350) { // ADD
    for (int i = 0; i < count; i++) {
      if (pendingQty[i] > 0) {
        addToCart(activeCatalogType, catalog[i].id, catalog[i].name, catalogDisplayPrice(i), pendingQty[i]);
      }
    }
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }

  if (x >= 140 && x <= 240 && y >= 270 && y <= 350) { // CANCEL
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }
}

void handleCartTouch(int x, int y) {
  if (cartCount > 0) {
    int rowH = cartRowHeight();
    for (int i = 0; i < cartCount; i++) {
      int rowY = cartRowY(i);
      int btnSize = min(rowH - 6, 40);
      int btnY = rowY + (rowH - btnSize) / 2;

      if (x >= 192 && x <= 196 + btnSize + 4 && y >= btnY - 4 && y <= btnY + btnSize + 4) {
        removeFromCart(i);
        drawCartScreen();
        return;
      }
    }
  }

  if (x >= 20 && x <= 230 && y >= 320 && y <= 340) { // BACK
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
  }
}

void tftUiBegin() {
  tft.begin();
  tft.setRotation(2);
  diagTouchOk = ts.begin();
  ts.setRotation(0);
  resetPendingSelections();
  currentScreen = (credits > 0) ? SCREEN_MAIN : SCREEN_IDLE;
  redrawCurrentScreen();
}

void tftUiSetCredits() {
  bool hasCredits = credits > 0;
  if (currentScreen == SCREEN_IDLE && hasCredits) {
    currentScreen = SCREEN_MAIN;
    redrawCurrentScreen();
  } else if (currentScreen != SCREEN_IDLE && !hasCredits && !orderInProgress) {
    currentScreen = SCREEN_IDLE;
    cartCount = 0;
    redrawCurrentScreen();
  } else {
    drawTftStatusBar();
  }
}

void tftUiSetWifiStatus(int status) {
  bool changed = wifiStatus != status;
  wifiStatus = (WifiStatus)status;
  uiWifiConnected = (status == WIFI_STATUS_CONNECTED);
  if (!orderInProgress) refreshMachineAvailability(changed);
  if (currentScreen == SCREEN_IDLE || currentScreen == SCREEN_MAIN) {
    redrawCurrentScreen();
  } else {
    drawTftStatusBar();
  }
}

void tftUiSetWifiConnected(bool connected) {
  tftUiSetWifiStatus(connected ? WIFI_STATUS_CONNECTED : WIFI_STATUS_IDLE);
}

void drawWifiSpinnerFrame() {
  tft.fillRect(tft.width() / 2 - 10, 190, 20, 20, COL_BLACK);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  char buf[2] = { SPINNER_CHARS[spinnerFrame % 4], '\0' };
  printCentered(buf, tft.width() / 2, 200);
  spinnerFrame++;
}

void tftUiShowError(String message) {
  uiErrorUntil = millis() + 2500;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_RED);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 24);
}

void tftUiShowSuccess(String message) {
  uiSuccessUntil = millis() + 2000;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_DARKGREEN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 24);
}

void tftUiLoop() {
  unsigned long now = millis();
  if (now < touchDebounceUntil) return;
  if (!ts.touched()) return;
  touchDebounceUntil = now + 200;

  TS_Point p = ts.getPoint();
  int rawX = p.x, rawY = p.y;
#if TOUCH_SWAP_XY
  int tmp = rawX; rawX = rawY; rawY = tmp;
#endif
  int x = map(rawX, TS_MINX, TS_MAXX, 0, tft.width());
  int y = map(rawY, TS_MINY, TS_MAXY, 0, tft.height());
#if TOUCH_INVERT_X
  x = tft.width() - x;
#endif
#if TOUCH_INVERT_Y
  y = tft.height() - y;
#endif

  Serial.print("Touch at x="); Serial.print(x); Serial.print(" y="); Serial.println(y);

  switch (currentScreen) {
    case SCREEN_MAIN:        handleMainTouch(x, y);        break;
    case SCREEN_PAPER_BRAND: handlePaperBrandTouch(x, y);   break;
    case SCREEN_CATALOG:     handleCatalogTouch(x, y);     break;
    case SCREEN_CART:        handleCartTouch(x, y);        break;
    default: break;
  }
}

// ================= UNO SERIAL MESSAGES (L5290 PRESENCE & STATUS) =================
// ================= CATALOG SYNC FROM ESP32 =================
// Handles: PAPER_BAY:<bay>:<prod_id>:<presence>:<sheets>:<price_cents>:<name>
void parsePaperBay(String msg) {
  // Strip prefix
  String data = msg.substring(10); // after "PAPER_BAY:"
  int p1 = data.indexOf(':');
  int p2 = data.indexOf(':', p1 + 1);
  int p3 = data.indexOf(':', p2 + 1);
  int p4 = data.indexOf(':', p3 + 1);
  int p5 = data.indexOf(':', p4 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) return;

  int bayNum   = data.substring(0, p1).toInt();          // 1-4
  int prodId   = data.substring(p1 + 1, p2).toInt();
  String pres  = data.substring(p2 + 1, p3);             // HIGH or LOW
  // p3..p4 = sheets (unused for display but kept)
  int priceCents = data.substring(p4 + 1, p5).toInt();
  String name  = data.substring(p5 + 1);
  name.trim();

  int idx = bayNum - 1;
  if (idx < 0 || idx >= PAPER_COUNT) return;

  paperCatalog[idx].id    = prodId;
  paperCatalog[idx].price = priceCents / 100.0;
  paperCatalog[idx].isPaperPresent = (pres == "HIGH");
  name.toCharArray(paperCatalogNames[idx], 32);
  paperCatalog[idx].name = paperCatalogNames[idx];

  Serial.print("Catalog Sync Paper Bay "); Serial.print(bayNum);
  Serial.print(": "); Serial.print(name);
  Serial.print(" P"); Serial.print(priceCents / 100.0, 2);
  Serial.print(" ["); Serial.print(pres); Serial.println("]");

  if (currentScreen == SCREEN_CATALOG && activeCatalogType == "paper") {
    drawCatalogScreen();
  }
}

// Handles: PEN_BAY:<bay>:<prod_id>:<stock>:<price_cents>:<name>
void parsePenBay(String msg) {
  String data = msg.substring(8); // after "PEN_BAY:"
  int p1 = data.indexOf(':');
  int p2 = data.indexOf(':', p1 + 1);
  int p3 = data.indexOf(':', p2 + 1);
  int p4 = data.indexOf(':', p3 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return;

  int bayNum     = data.substring(0, p1).toInt();        // 1-3
  int prodId     = data.substring(p1 + 1, p2).toInt();
  int stock      = data.substring(p2 + 1, p3).toInt();
  int priceCents = data.substring(p3 + 1, p4).toInt();
  String name    = data.substring(p4 + 1);
  name.trim();

  int idx = bayNum - 1;
  if (idx < 0 || idx >= BALLPEN_COUNT) return;

  ballpenCatalog[idx].id    = prodId;
  ballpenCatalog[idx].price = priceCents / 100.0;
  ballpenCatalog[idx].isPaperPresent = (stock > 0); // available if stock > 0
  name.toCharArray(ballpenCatalogNames[idx], 32);
  ballpenCatalog[idx].name = ballpenCatalogNames[idx];

  Serial.print("Catalog Sync Pen Bay "); Serial.print(bayNum);
  Serial.print(": "); Serial.print(name);
  Serial.print(" P"); Serial.print(priceCents / 100.0, 2);
  Serial.print(" Stock: "); Serial.println(stock);

  if (currentScreen == SCREEN_CATALOG && activeCatalogType == "pen") {
    drawCatalogScreen();
  }
}

void handleUnoMessage(String msg) {
  msg.trim();
  if (msg.startsWith("STATUS:")) {
    // Format: STATUS:HIGH,HIGH,LOW,HIGH
    // Uno reports live L5290 sensor states; update paperCatalog presence flags
    String list = msg.substring(7);
    int start = 0;
    for (int i = 0; i < 4; i++) {
      int comma = list.indexOf(',', start);
      String val = (comma == -1) ? list.substring(start) : list.substring(start, comma);
      paperCatalog[i].isPaperPresent = (val == "HIGH");
      if (comma == -1) break;
      start = comma + 1;
    }
    if (currentScreen == SCREEN_CATALOG && activeCatalogType == "paper") {
      drawCatalogScreen();
    }
  }
}

// Sends dispense command to Arduino Uno and waits for sensor confirmation
int dispensePaperFromUno(int bayNumber, int sheetCount) {
  if (bayNumber < 1 || bayNumber > 4) return 0;
  UNO_SERIAL.println("DISPENSE:" + String(bayNumber) + ":" + String(sheetCount));

  unsigned long startedAt = millis();
  while (millis() - startedAt < PAPER_DISPENSE_TIMEOUT_MS) {
    if (UNO_SERIAL.available()) {
      String response = UNO_SERIAL.readStringUntil('\n');
      response.trim();
      if (response.startsWith("DONE:")) {
        // Format: DONE:<bay>:<count>
        int second = response.indexOf(':', 5);
        int count = response.substring(second + 1).toInt();
        return count;
      }
      else if (response.startsWith("EMPTY:")) {
        // Format: EMPTY:<bay>:<count>
        int second = response.indexOf(':', 6);
        int count = (second > 0) ? response.substring(second + 1).toInt() : 0;
        paperCatalog[bayNumber - 1].isPaperPresent = false;
        CLOUD_SERIAL.println("BAY_EMPTY:" + String(bayNumber));
        return count;
      }
    }
  }
  return 0; // Timeout
}

// ================= DIAGNOSTICS =================
void runDiagnostics() {
  Serial.println();
  Serial.println("========== DIAGNOSTICS (OPTION A) ==========");
  Serial.print("Uptime: "); Serial.print(millis() / 1000); Serial.println("s");
  Serial.print("OLED (SH1106)............ "); Serial.println(diagOledOk ? "OK" : "FAIL");
  Serial.print("Touchscreen (XPT2046)..... "); Serial.println(diagTouchOk ? "OK" : "FAIL");
  Serial.print("Coin acceptor pin (D2).... "); Serial.println("INPUT_PULLUP + interrupt INT0 configured");
  Serial.println("Serial2 (Pins 16/17) ---> Arduino Uno Paper Controller connected at 9600 baud");

  Serial.println("--- Pen IR sensors (active LOW = beam broken) ---");
  for (int i = 0; i < 3; i++) {
    Serial.print("  Slot "); Serial.print(i + 1); Serial.print(" (D");
    Serial.print(penIrPins[i]); Serial.print("): ");
    Serial.println(digitalRead(penIrPins[i]) == HIGH ? "OK - beam clear" : "WARNING - LOW at idle");
  }

  Serial.println("============================================");
  UNO_SERIAL.println("STATUS?");
}

void printHardwareStatus() {
  Serial.println("--- HARDWARE STATUS ---");
  Serial.print("Credit: P"); Serial.println(credits);
  Serial.print("Order active: "); Serial.println(orderInProgress ? "YES" : "NO");
  Serial.print("Indicator: ");
  Serial.println(indicatorState == INDICATOR_READY ? "READY (green)" : indicatorState == INDICATOR_ACTIVE ? "ACTIVE (blue)" : "ERROR (red)");
  for (int i = 0; i < 3; i++) {
    Serial.print("Pen IR "); Serial.print(i + 1); Serial.print(" (D");
    Serial.print(penIrPins[i]); Serial.print("): ");
    Serial.println(digitalRead(penIrPins[i]) == LOW ? "LOW / blocked" : "HIGH / clear");
  }
  Serial.print("Hopper relay D14: "); Serial.println(digitalRead(CHANGE_HOPPER_MOTOR_PIN) == HOPPER_RELAY_ON ? "ON" : "OFF");
  Serial.print("Hopper sensor D15: "); Serial.println(digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW ? "LOW / blocked" : "HIGH / clear");
}

void softResetMachineState() {
  Serial.println("SOFT RESET: returning machine logic to idle state.");
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  for (int i = 0; i < 3; i++) stopStepper(i);

  noInterrupts();
  credits = 0;
  coinPulseReceived = false;
  interrupts();

  isProcessing = false;
  orderInProgress = false;
  activeTransactionId = "";
  activeChangeCents = 0;
  selectedPaperBrand = "Budget";
  activeCatalogType = "paper";
  cartCount = 0;
  cartDispenseIndex = 0;
  orderSummaryText = "";
  orderTotalCost = 0;
  resetPendingSelections();
  setCoinAcceptance(true);

  servoChange.write(0);
  servoPen.write(0);

  currentScreen = SCREEN_IDLE;
  refreshMachineAvailability(true);
  updateLCD();
  tftUiSetCredits();
  redrawCurrentScreen();
  CLOUD_SERIAL.println("CREDIT:0");
  CLOUD_SERIAL.println("SOFT_RESET");
  CLOUD_SERIAL.println("STATUS?");
  delay(300);
  CLOUD_SERIAL.println("GET_CATALOG");
  UNO_SERIAL.println("STATUS?");
  Serial.println("SOFT RESET: done.");
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  CLOUD_SERIAL.begin(9600); // UART to ESP32 (Pins 18/19)
  UNO_SERIAL.begin(9600);   // UART to Arduino Uno (Pins 16/17)
  Serial.println("--- REVAMPED SMART PAPER VENDO FIRMWARE (OPTION A) STARTING ---");

  for (int i = 0; i < 3; i++) penSteppers[i]->setSpeed(10);

  Wire.begin();
  Wire.setWireTimeout(25000, true);

  if (!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println("OLED SH1106 allocation failed");
    while (true);
  }
  diagOledOk = true;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0); display.print("Smart Vendo V3");
  display.display();

  tftUiBegin();

  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  setMachineIndicator(INDICATOR_ACTIVE, true);

  pinMode(HW_RESET_BTN_PIN, INPUT_PULLUP);
  pinMode(SW_RESET_BTN_PIN, INPUT_PULLUP);

  pinMode(COIN_PIN, INPUT_PULLUP);
  pinMode(COIN_INHIBIT_PIN, OUTPUT);
  setCoinAcceptance(true);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);

  pinMode(PEN_IR_PIN, INPUT_PULLUP);
  pinMode(PEN_IR_PIN2, INPUT_PULLUP);
  pinMode(PEN_IR_PIN3, INPUT_PULLUP);

  pinMode(CHANGE_HOPPER_MOTOR_PIN, OUTPUT);
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  pinMode(CHANGE_HOPPER_SENSOR_PIN, INPUT_PULLUP);

  servoChange.attach(SERVO_CHANGE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  servoChange.write(0);
  servoPen.write(0);

  updateLCD();
  CLOUD_SERIAL.println("CREDIT:" + String((unsigned int)credits));
  CLOUD_SERIAL.println("STATUS?");
  // Request live catalog from ESP32 after boot so TFT shows real bay assignments
  delay(500);
  CLOUD_SERIAL.println("GET_CATALOG");
  UNO_SERIAL.println("STATUS?");
  runDiagnostics();
}

void coinInterrupt() {
  if (orderInProgress) return;
  static unsigned long lastPulse = 0;
  unsigned long now = millis();
  if (now - lastPulse > 50) {
    credits++;
    coinPulseReceived = true;
    lastPulse = now;
  }
}

void loop() {
  unsigned long now = millis();

  // HW RESET (A8)
  if (digitalRead(HW_RESET_BTN_PIN) == LOW && now >= hwResetDebounceUntil) {
    hwResetDebounceUntil = now + 1000;
    Serial.println("HW RESET BUTTON (A8): triggering watchdog reboot...");
    Serial.flush();
    setMachineIndicator(INDICATOR_ERROR, false);
    tone(BUZZER_PIN, 500, 200);
    delay(200);
    noInterrupts();
    wdt_enable(WDTO_15MS);
    while (true) {}
  }

  // SW RESET (A9)
  if (digitalRead(SW_RESET_BTN_PIN) == LOW && now >= swResetDebounceUntil) {
    swResetDebounceUntil = now + 500;
    Serial.println("SW RESET BUTTON (A9): performing software reset...");
    softResetMachineState();
  }

  tftUiLoop();

  if (currentScreen == SCREEN_IDLE && wifiStatus == WIFI_STATUS_CONNECTING) {
    if (millis() - lastSpinnerUpdate > 200) {
      lastSpinnerUpdate = millis();
      drawWifiSpinnerFrame();
    }
  }

  if (hopperManualRunning && millis() - hopperManualStartedAt >= HOPPER_MANUAL_MAX_MS) {
    digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
    hopperManualRunning = false;
  }

  static uint16_t lastCredits = 65535;
  noInterrupts();
  uint16_t creditSnapshot = credits;
  bool pulseSnapshot = coinPulseReceived;
  coinPulseReceived = false;
  interrupts();

  if (pulseSnapshot) {
    Serial.println("Coin pulse detected on D2.");
  }

  if (creditSnapshot != lastCredits) {
    lastCredits = creditSnapshot;
    updateLCD();
    tftUiSetCredits();
    CLOUD_SERIAL.println("CREDIT:" + String((unsigned int)creditSnapshot));
    Serial.println("Credits inserted! Total: P" + String((unsigned int)creditSnapshot));
  }

  if (CLOUD_SERIAL.available()) {
    String msg = CLOUD_SERIAL.readStringUntil('\n');
    msg.trim();
    handleCloudCommand(msg);
  }

  if (UNO_SERIAL.available()) {
    String msg = UNO_SERIAL.readStringUntil('\n');
    msg.trim();
    handleUnoMessage(msg);
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "DIAG") runDiagnostics();
    else if (cmd == "STATUS") printHardwareStatus();
    else if (cmd == "SOFT_RESET") softResetMachineState();
    else if (cmd == "ESP_RESET") CLOUD_SERIAL.println("ESP_RESET");
    else if (cmd == "HOPPER ON") {
      digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
      hopperManualRunning = true;
      hopperManualStartedAt = millis();
    } else if (cmd == "HOPPER OFF") {
      digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
      hopperManualRunning = false;
    } else if (cmd.startsWith("HOPPER ")) {
      int coins = cmd.substring(7).toInt();
      if (coins > 0) releaseVerifiedChange(coins * 100);
    }
  }
}

bool dispenseOnePen(int channel) {
  int penIndex = channel - 1;
  if (penIndex < 0 || penIndex > 2) return false;
  Stepper* pen = penSteppers[penIndex];
  int irPin = penIrPins[penIndex];
  if (digitalRead(irPin) == LOW) {
    Serial.println("PEN ABORT: IR is LOW before dispense; chute blocked.");
    return false;
  }
  pen->step(1024);
  unsigned long startedAt = millis();
  bool detected = false;
  while (millis() - startedAt < PEN_SENSOR_TIMEOUT_MS) {
    if (digitalRead(irPin) == LOW) { detected = true; break; }
  }
  delay(300);
  pen->step(-1024);
  stopStepper(penIndex);
  return detected;
}

void stopStepper(int penIndex) {
  for (int p = 0; p < 4; p++) digitalWrite(penStopPins[penIndex][p], LOW);
}

int releaseVerifiedChange(int changeCents) {
  if (changeCents == 0) return 0;
  if (changeCents % 100 != 0) return -1;
  if (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW) {
    Serial.println("HOPPER ABORT: exit sensor is LOW; clear it first.");
    return -1;
  }
  const int expectedCoins = changeCents / 100;
  int countedCoins = 0;
  bool previousBlocked = false;
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
  unsigned long lastCoinAt = millis();
  while (countedCoins < expectedCoins && millis() - lastCoinAt < CHANGE_COIN_TIMEOUT_MS) {
    bool blocked = (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW);
    if (blocked && !previousBlocked) {
      countedCoins++;
      lastCoinAt = millis();
    }
    previousBlocked = blocked;
  }
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  return (countedCoins == expectedCoins) ? countedCoins * 100 : -1;
}

void executeDispensePlan(String message) {
  int first = message.indexOf(':');
  int second = message.indexOf(':', first + 1);
  if (first < 0 || second < 0) { showError("Bad dispense plan"); return; }
  const String transactionId = message.substring(first + 1, second);
  const String encodedPlan = message.substring(second + 1);
  if (transactionId != activeTransactionId) { showError("Wrong transaction"); return; }

  String results = "";
  int start = 0;
  while (start < encodedPlan.length()) {
    int end = encodedPlan.indexOf(';', start);
    String line = end < 0 ? encodedPlan.substring(start) : encodedPlan.substring(start, end);
    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    if (p1 <= 0 || p2 <= p1 || p3 <= p2) { showError("Bad plan line"); return; }
    String type = line.substring(0, p1);
    int productId = line.substring(p1 + 1, p2).toInt();
    int channel = line.substring(p2 + 1, p3).toInt();
    int expectedOutput = line.substring(p3 + 1).toInt();
    int actualOutput = 0;

    if (type == "paper") {
      // Delegate 4-bay paper dispense to Arduino Uno
      actualOutput = dispensePaperFromUno(channel, expectedOutput);
    } else {
      // Dispense pens directly on Mega
      for (int item = 0; item < expectedOutput; item++) {
        bool released = dispenseOnePen(channel);
        if (!released) break;
        actualOutput++;
      }
    }

    if (results.length()) results += ';';
    results += type + "," + String(productId) + "," + String(actualOutput);
    if (end < 0) break;
    start = end + 1;
  }
  CLOUD_SERIAL.println("FINISH:" + activeTransactionId + ":" + results);
}

void beginReservedTransaction(String message) {
  int first = message.indexOf(':');
  int second = message.indexOf(':', first + 1);
  int third = message.indexOf(':', second + 1);
  if (first < 0 || second < 0 || third < 0) { showError("Bad reservation"); return; }
  activeTransactionId = message.substring(first + 1, second);
  orderTotalCost = message.substring(second + 1, third).toInt() / 100.0;
  activeChangeCents = message.substring(third + 1).toInt();
  orderSummaryText = "Reserved\nChange: P" + String(activeChangeCents / 100.0, 2);
  drawSummaryScreen();

  int verifiedChange = releaseVerifiedChange(activeChangeCents);
  if (verifiedChange < 0) {
    CLOUD_SERIAL.println("CHANGE_FAIL:" + activeTransactionId + ":HOPPER_SENSOR_TIMEOUT");
    showError("Change not released");
    return;
  }
  CLOUD_SERIAL.println("CHANGE_OK:" + activeTransactionId + ":" + String(verifiedChange));
}

void finishUiAfterTransaction(String message) {
  bool completed = message.endsWith(":COMPLETED");
  credits = 0;
  orderInProgress = false;
  setCoinAcceptance(true);
  cartCount = 0;
  activeTransactionId = "";
  updateLCD();
  if (completed) {
    refreshMachineAvailability(true);
    tftUiShowSuccess("Take items");
    currentScreen = SCREEN_IDLE;
  } else {
    showError("Partial dispense");
  }
  redrawCurrentScreen();
}

void handleCloudCommand(String msg) {
  if (msg.startsWith("RESERVED:")) beginReservedTransaction(msg);
  else if (msg.startsWith("PLAN:")) executeDispensePlan(msg);
  else if (msg.startsWith("FINISHED:")) finishUiAfterTransaction(msg);
  else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  // ── Dynamic catalog sync from ESP32 ──────────────────────────
  else if (msg.startsWith("PAPER_BAY:")) parsePaperBay(msg);
  else if (msg.startsWith("PEN_BAY:"))   parsePenBay(msg);
  // ─────────────────────────────────────────────────────────────
  else if (msg.startsWith("WIFI:")) {
    bool connected = msg.substring(5) == "1";
    tftUiSetWifiConnected(connected);
  }
  else if (msg == "WIFISTATE:CONNECTING") {
    tftUiSetWifiStatus(WIFI_STATUS_CONNECTING);
  }
  else if (msg == "WIFISTATE:NOTFOUND") {
    tftUiSetWifiStatus(WIFI_STATUS_NOT_FOUND);
  }
}

void updateLCD() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print("P");
  display.print((int)credits);

  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("Ready to Serve");
  display.display();
}

void showError(String m) {
  setMachineIndicator(INDICATOR_ERROR, true);
  drawStatusScreen("Error", m);
  tftUiShowError(m);
  if (orderInProgress) {
    orderInProgress = false;
    setCoinAcceptance(true);
    cartCount = 0;
    currentScreen = SCREEN_MAIN;
  }
  delay(2000);
  refreshMachineAvailability();
  isProcessing = false;
  updateLCD();
  redrawCurrentScreen();
}

void drawStatusScreen(String headline, String message) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(headline);

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print(message);
  display.display();
}

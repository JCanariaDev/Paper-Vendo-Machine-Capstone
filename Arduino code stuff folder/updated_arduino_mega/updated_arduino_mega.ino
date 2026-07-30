#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Servo.h>
#include <HX711.h>
#include <Stepper.h>
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>

/*
  renew5.ino - Master Controller for Paper & Pen Vendo
  REVISION 2: Full touchscreen UI flow (idle -> main menu -> catalog
  selection with checkboxes/qty steppers -> cart -> order summary ->
  dispense change), replacing the old 3-button ITEM1/ITEM2/CONFIRM demo.

  REVISION 5: Added 2 more stepper motors (3 total) so each of the 3
  ballpen catalog slots has its own dedicated dispenser + IR sensor,
  instead of sharing a single motor.

  REVISION 6 - 29/07/2026: Added onboard diagnostics. Runs automatically
  once at startup and can be re-run any time by typing DIAG into the
  Serial Monitor (USB serial, separate from the CLOUD_SERIAL line to the
  ESP32). Type DIAG:MOVE1 / DIAG:MOVE2 / DIAG:MOVE3 to jog a specific pen
  stepper a small amount as a physical sanity check.

  Install via Library Manager if missing:
    - Adafruit ILI9341
    - Adafruit GFX Library (dependency)
    - XPT2046_Touchscreen (by Paul Stoffregen)
*/

// --- PINS (existing) ---
const int COIN_PIN = 2;
const int LOADCELL_DOUT = 5;
const int LOADCELL_SCK = 6;
const int PEN_IR_PIN = 7;   // pen slot 1 IR sensor
const int PEN_IR_PIN2 = 30; // pen slot 2 IR sensor - NEW
const int PEN_IR_PIN3 = 31; // pen slot 3 IR sensor - NEW
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;
// Stepper Pins: 3, 4, 11, 12 (pen slot 1)
//               22, 23, 24, 25 (pen slot 2) - NEW
//               26, 27, 28, 29 (pen slot 3) - NEW

// --- PINS (touchscreen) ---
// SPI bus (52/51/50) is fixed hardware SPI on the Mega - not redefinable.
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

// --- STEPPER CONFIG ---
// 3 independent pen dispensers - one stepper motor + one IR sensor per
// ballpen catalog slot. Wiring order per Stepper object is
// (steps, pin1, pin3, pin2, pin4) - kept the same pattern for the two
// new motors to match how the original one is wired.
const int stepsPerRevolution = 2048;

Stepper penStepper1(stepsPerRevolution, 3, 11, 4, 12);   // existing motor - unchanged
Stepper penStepper2(stepsPerRevolution, 22, 24, 23, 25); // NEW - ballpen slot 2
Stepper penStepper3(stepsPerRevolution, 26, 28, 27, 29); // NEW - ballpen slot 3

// Index 0/1/2 = ballpen catalog id 1/2/3
Stepper* penSteppers[3] = { &penStepper1, &penStepper2, &penStepper3 };
const int penStopPins[3][4] = {
  { 3,  4,  11, 12 },
  { 22, 23, 24, 25 },
  { 26, 27, 28, 29 }
};
const int penIrPins[3] = { PEN_IR_PIN, PEN_IR_PIN2, PEN_IR_PIN3 };

// --- PERIPHERALS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#if defined(HAVE_HWSERIAL1)
#define CLOUD_SERIAL Serial1
#else
#define CLOUD_SERIAL Serial
#endif

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo servoChange, servoPen;
HX711 scale;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
XPT2046_Touchscreen ts(TOUCH_CS);

// --- TOUCH CALIBRATION ---
// Pulled the raw X/Y ranges from your TFT_eSPI calData {328,3531,336,3434,7}.
// The trailing "7" was TFT_eSPI's internal orientation bitmask and doesn't
// apply to XPT2046_Touchscreen's simpler raw-value mapping, so it's dropped.
#define TS_MINX 328
#define TS_MAXX 3531
#define TS_MINY 336
#define TS_MAXY 3434

// Fix for "tapping the top button triggers the bottom one": flip these
// 0/1 toggles and re-upload if touch still feels off/inverted. Try
// TOUCH_INVERT_Y first (most common fix for this symptom); if it's still
// wrong, try TOUCH_SWAP_XY or TOUCH_INVERT_X instead.
#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  1

// Simple RGB565 color constants
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
String currentRequestId = ""; // Track the selected item's ID for transaction logging
String currentRequestType = "";

// --- DIAGNOSTICS STATE ---
bool diagOledOk = false;
bool diagTftOk = false;
bool diagTouchOk = false;
bool diagScaleOk = false;

// ================= CATALOG =================
// EDIT these to exactly match your Supabase paper_settings / ballpen_settings
// row ids. price here is just a display estimate - the real charge is still
// validated against Supabase when the order is sent to the ESP32.
// NOTE: layout below supports up to 3 rows per catalog screen comfortably -
// more than that will overlap the Add/Cancel buttons; shrink row height or
// add scrolling if you need a bigger catalog later.

struct CatalogItem {
  int id;
  const char* name;
  float price;
};

const int PAPER_COUNT = 3;
CatalogItem paperCatalog[PAPER_COUNT] = {
  {1, "Short (Letter)", 1.00},
  {2, "Long (Legal)",   1.50},
  {3, "A4",             1.25}
};

const int BALLPEN_COUNT = 3;
CatalogItem ballpenCatalog[BALLPEN_COUNT] = {
  {1, "Black Ballpen", 5.00},
  {2, "Blue Ballpen",  5.00},
  {3, "Red Ballpen",   5.00}
};

const int MAX_CATALOG_ROWS = 3; // must be >= max(PAPER_COUNT, BALLPEN_COUNT)
int pendingQty[MAX_CATALOG_ROWS];
bool pendingChecked[MAX_CATALOG_ROWS];
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
enum UiScreen { SCREEN_IDLE, SCREEN_MAIN, SCREEN_CATALOG, SCREEN_SUMMARY };
UiScreen currentScreen = SCREEN_IDLE;

bool uiWifiConnected = false;
unsigned long uiErrorUntil = 0;
unsigned long uiSuccessUntil = 0;
unsigned long touchDebounceUntil = 0;

bool orderInProgress = false;
int cartDispenseIndex = 0;
String orderSummaryText = "";
float orderTotalCost = 0;

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
    pendingChecked[i] = false;
  }
}

void drawIdleScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Insert Coins", tft.width() / 2, 140);
  printCentered("to use.", tft.width() / 2, 165);
}

void drawMainScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Paper and Ballpen", tft.width() / 2, 45);
  printCentered("Vending Machine", tft.width() / 2, 65);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  printCentered("PAPER", tft.width() / 2, 95 + 27);

  tft.fillRoundRect(20, 160, 200, 55, 8, COL_GREEN);
  printCentered("BALLPEN", tft.width() / 2, 160 + 27);

  tft.fillRoundRect(20, 225, 200, 55, 8, COL_ORANGE);
  tft.setTextSize(1);
  if (cartCount > 0) {
    String label = "COMPLETE (P" + String(cartTotal(), 2) + ")";
    printCentered(label.c_str(), tft.width() / 2, 225 + 27);
  } else {
    printCentered("COMPLETE TRANSACTION", tft.width() / 2, 225 + 27);
  }
}

void drawCatalogScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered((activeCatalogType == "paper") ? "Paper Options" : "Ballpen Options", tft.width() / 2, 40);

  for (int i = 0; i < count; i++) {
    int rowY = 65 + i * 55;

    tft.fillRoundRect(8, rowY, 30, 40, 6, COL_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(2);
    printCentered("-", 23, rowY + 20);

    char qtyBuf[4];
    sprintf(qtyBuf, "%d", pendingQty[i]);
    printCentered(qtyBuf, 56, rowY + 20);

    tft.fillRoundRect(74, rowY, 30, 40, 6, COL_GREEN);
    printCentered("+", 89, rowY + 20);

    tft.setTextColor(COL_WHITE);
    tft.setTextSize(1);
    tft.setCursor(112, rowY + 4);
    tft.print(catalog[i].name);
    tft.setCursor(112, rowY + 22);
    tft.print("P" + String(catalog[i].price, 2));

    tft.drawRect(200, rowY + 8, 24, 24, COL_WHITE);
    if (pendingChecked[i]) {
      tft.fillRect(204, rowY + 12, 16, 16, COL_GREEN);
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
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_ORANGE);
    tft.setTextColor(COL_WHITE);
    printCentered("DISPENSE CHANGE", tft.width() / 2, 275 + 20);
  }
}

void redrawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_IDLE:    drawIdleScreen();    break;
    case SCREEN_MAIN:    drawMainScreen();    break;
    case SCREEN_CATALOG: drawCatalogScreen(); break;
    case SCREEN_SUMMARY: drawSummaryScreen(); break;
  }
}

// ================= CART / ORDER LOGIC =================

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

void sendNextCartItem() {
  CartItem &item = cart[cartDispenseIndex];
  CLOUD_SERIAL.println("TFTORDER:" + item.type + ":" + String(item.id) + ":" + String(item.qty));
}

void startOrder() {
  orderInProgress = true;
  cartDispenseIndex = 0;
  orderSummaryText = "";
  orderTotalCost = 0;
  currentScreen = SCREEN_SUMMARY;
  drawSummaryScreen();
  sendNextCartItem();
}

// ================= TOUCH HANDLERS =================

void handleMainTouch(int x, int y) {
  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    activeCatalogType = "paper";
    resetPendingSelections();
    currentScreen = SCREEN_CATALOG;
    drawCatalogScreen();
  } else if (x >= 20 && x <= 220 && y >= 160 && y <= 215) {
    activeCatalogType = "ballpen";
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

void handleCatalogTouch(int x, int y) {
  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  for (int i = 0; i < count; i++) {
    int rowY = 65 + i * 55;
    if (x >= 8 && x <= 38 && y >= rowY && y <= rowY + 40) {
      pendingQty[i] = max(0, pendingQty[i] - 1);
      if (pendingQty[i] == 0) pendingChecked[i] = false;
      drawCatalogScreen();
      return;
    }
    if (x >= 74 && x <= 104 && y >= rowY && y <= rowY + 40) {
      pendingQty[i] = min(20, pendingQty[i] + 1);
      pendingChecked[i] = true;
      drawCatalogScreen();
      return;
    }
    if (x >= 200 && x <= 224 && y >= rowY + 8 && y <= rowY + 32) {
      pendingChecked[i] = !pendingChecked[i];
      pendingQty[i] = pendingChecked[i] ? max(1, pendingQty[i]) : 0;
      drawCatalogScreen();
      return;
    }
  }

  if (x >= 15 && x <= 110 && y >= 275 && y <= 310) { // ADD
    for (int i = 0; i < count; i++) {
      if (pendingChecked[i] && pendingQty[i] > 0) {
        addToCart(activeCatalogType, catalog[i].id, catalog[i].name, catalog[i].price, pendingQty[i]);
      }
    }
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }

  if (x >= 130 && x <= 225 && y >= 275 && y <= 310) { // CANCEL
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }
}

void handleSummaryTouch(int x, int y) {
  if (orderInProgress) return;
  if (x >= 20 && x <= 220 && y >= 275 && y <= 315) {
    returnChange(); // sets credits to 0, which auto-switches back to idle
  }
}

// ================= TFT UI ENTRY POINTS =================

void tftUiBegin() {
  tft.begin();
  tft.setRotation(0);
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

void tftUiSetWifiConnected(bool connected) {
  uiWifiConnected = connected;
  drawTftStatusBar();
}

void tftUiShowError(String message) {
  uiErrorUntil = millis() + 2500;
  tft.fillRect(0, tft.height() - 24, tft.width(), 24, COL_RED);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 12);
}

void tftUiShowSuccess(String message) {
  uiSuccessUntil = millis() + 2000;
  tft.fillRect(0, tft.height() - 24, tft.width(), 24, COL_DARKGREEN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 12);
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

  switch (currentScreen) {
    case SCREEN_MAIN:    handleMainTouch(x, y);    break;
    case SCREEN_CATALOG: handleCatalogTouch(x, y); break;
    case SCREEN_SUMMARY: handleSummaryTouch(x, y); break;
    default: break; // SCREEN_IDLE has nothing to tap
  }
}

// ================= DIAGNOSTICS =================
// Type DIAG into the Serial Monitor (USB serial) any time to re-run this.
// Note: the Mega has no RTC, so "uptime" is all we can report - not a
// real date/time.
void runDiagnostics() {
  Serial.println();
  Serial.println("========== DIAGNOSTICS ==========");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println("s");

  Serial.print("OLED (SH1106)............ ");
  Serial.println(diagOledOk ? "OK" : "FAIL - not detected at startup");

  Serial.print("TFT (ILI9341)............ ");
  uint8_t tftId = tft.readcommand8(0x04, 1); // RDDID, 2nd param byte
  diagTftOk = (tftId != 0x00 && tftId != 0xFF);
  Serial.print(diagTftOk ? "OK" : "FAIL - no response");
  Serial.print(" (ID byte: 0x");
  Serial.print(tftId, HEX);
  Serial.println(")");

  Serial.print("Touchscreen (XPT2046)..... ");
  Serial.println(diagTouchOk ? "OK" : "FAIL - ts.begin() returned false");

  Serial.print("Load cell (HX711)......... ");
  unsigned long scaleStart = millis();
  bool scaleReady = false;
  while (millis() - scaleStart < 500) {
    if (scale.is_ready()) { scaleReady = true; break; }
  }
  diagScaleOk = scaleReady;
  Serial.println(diagScaleOk ? "OK" : "NOT RESPONDING - normal if not wired up yet");

  Serial.print("Coin acceptor pin (D2).... ");
  Serial.println("configured (INPUT_PULLUP + interrupt) - insert a coin to confirm live");

  Serial.println("Servos (D9 change / D10 pen) - attached, no electrical feedback available");

  Serial.println("--- Pen IR sensors (active LOW = beam broken) ---");
  for (int i = 0; i < 3; i++) {
    int level = digitalRead(penIrPins[i]);
    Serial.print("  Slot "); Serial.print(i + 1);
    Serial.print(" (pin "); Serial.print(penIrPins[i]); Serial.print(")... ");
    Serial.println(level == HIGH ? "OK - beam clear" : "WARNING - reading LOW at idle, check wiring/alignment");
  }

  Serial.println("--- Pen steppers (pins only - no position feedback) ---");
  for (int i = 0; i < 3; i++) {
    Serial.print("  Slot "); Serial.print(i + 1); Serial.print(": pins ");
    for (int p = 0; p < 4; p++) {
      Serial.print(penStopPins[i][p]);
      if (p < 3) Serial.print(",");
    }
    Serial.print(" - configured. Send DIAG:MOVE"); Serial.print(i + 1);
    Serial.println(" to jog it and confirm visually.");
  }

  Serial.println("==================================");
}

// ================= EXISTING MACHINE LOGIC =================

void setup() {
  Serial.begin(115200);
  #if defined(HAVE_HWSERIAL1)
  CLOUD_SERIAL.begin(9600); // To ESP32
  #endif
  Serial.println("--- SYSTEM STARTING ---");

  for (int i = 0; i < 3; i++) penSteppers[i]->setSpeed(10); // 10 RPM each - fixes vibration, keeps torque
  Serial.println("3 pen steppers configured (10 RPM each).");

  Wire.begin();
  Wire.setWireTimeout(25000, true);
  Serial.println("Starting OLED...");

  if (!display.begin(SCREEN_ADDRESS, true)) {
    Serial.println("OLED SH1106 allocation failed");
    while (true);
  }
  diagOledOk = true;
  Serial.println("OLED initialized.");
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0,0); display.print("Smart Vendo V3");
  display.display();
  Serial.println("OLED startup screen drawn.");

  Serial.println("Starting touchscreen UI...");
  tftUiBegin();
  Serial.println("Touchscreen UI initialized.");

  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);
  pinMode(PEN_IR_PIN, INPUT);
  pinMode(PEN_IR_PIN2, INPUT);
  pinMode(PEN_IR_PIN3, INPUT);

  servoChange.attach(SERVO_CHANGE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  servoChange.write(0);
  servoPen.write(0);
  Serial.println("Servos configured.");

  // The HX711 is not needed for coin counting. Leave it out of startup while
  // testing because a faulty module or wiring can stop the controller here.
  Serial.println("HX711 startup skipped for coin/OLED test.");

  Serial.println("Machine Ready!");
  updateLCD();
  CLOUD_SERIAL.println("CREDIT:" + String((unsigned int)credits));

  runDiagnostics(); // auto-run once at boot; type DIAG later to re-run
}

void coinInterrupt() {
  static unsigned long lastPulse = 0;
  unsigned long now = millis();
  if (now - lastPulse > 50) {
    credits++;
    coinPulseReceived = true;
    lastPulse = now;
  }
}

void loop() {
  tftUiLoop();

  // --- Check if coins were inserted ---
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

  // --- Diagnostics commands, typed into the Serial Monitor (USB serial) ---
  // Separate from CLOUD_SERIAL, so this never interferes with ESP32 traffic.
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "DIAG") {
      runDiagnostics();
    } else if (cmd.startsWith("DIAG:MOVE")) {
      int slot = cmd.substring(9).toInt(); // DIAG:MOVE1/2/3
      int idx = constrain(slot - 1, 0, 2);
      Serial.print("Jogging pen slot "); Serial.println(slot);
      penSteppers[idx]->step(200);
      delay(300);
      penSteppers[idx]->step(-200);
      stopStepper(idx);
      Serial.println("Done - did slot " + String(slot) + " visibly move?");
    }
  }
}

void handleCloudCommand(String msg) {
  if (msg.startsWith("DISPENSE:")) performDispense(msg);
  else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  else if (msg.startsWith("WIFI:")) tftUiSetWifiConnected(msg.substring(5) == "1");
  else if (msg == "RETURN_CHANGE") returnChange();
  else if (msg == "TEST_STEPPER_FWD") {
    penSteppers[0]->step(20);
    stopStepper(0);
  }
  else if (msg == "TEST_STEPPER_BACK") {
    penSteppers[0]->step(-20);
    stopStepper(0);
  }
}

void performDispense(String msg) {
  int f1 = msg.indexOf(':');
  int f2 = msg.indexOf(':', f1 + 1);
  int f3 = msg.indexOf(':', f2 + 1);

  String firstValue = msg.substring(f1 + 1, f2);
  int totalSheets = 0;
  float cost = 0;
  String name = "";
  String type = "";
  String id = "";

  // New ESP32 UI format: DISPENSE:TYPE:ID:QTY_OR_SHEETS:COST:NAME
  if (firstValue == "paper" || firstValue == "pen") {
    int f4 = msg.indexOf(':', f3 + 1);
    int f5 = msg.indexOf(':', f4 + 1);
    type = firstValue;
    id = msg.substring(f2 + 1, f3);
    totalSheets = msg.substring(f3 + 1, f4).toInt();
    cost = msg.substring(f4 + 1, f5).toFloat();
    name = msg.substring(f5 + 1);
  }
  // Legacy format: DISPENSE:SHEETS:COST:NAME
  else {
    type = "";
    id = currentRequestId;
    totalSheets = firstValue.toInt();
    cost = msg.substring(f2 + 1, f3).toFloat();
    name = msg.substring(f3 + 1);
  }

  currentRequestType = type;
  currentRequestId = id;

  drawStatusScreen("Dispensing", "Please wait");
  Serial.println("Received from Cloud: DISPENSE " + String(totalSheets) + " of " + name);

  bool isPaper = (type == "paper") || (type == "" && totalSheets > 1);
  int actualQty = totalSheets;

  if (isPaper) {
    // Paper logic is paused since Stepper is now used for the Pen Dispenser -
    // this logs the transaction but does not physically dispense yet.
    Serial.println("Paper requested, logging DONE to cloud...");
    CLOUD_SERIAL.println("DONE:paper:" + currentRequestId + ":" + name + ":" + String(cost) + ":" + String(totalSheets));
  } else {
    // Stepper Motor Pen Dispenser (Oscillating / Return Mechanism)
    // 1024 steps = 180 degree turn (Top to Bottom)
    // Each ballpen catalog id (1/2/3) has its own physical stepper + IR
    // sensor, so pick the matching one based on the id that was ordered.
    int penIndex = constrain(id.toInt() - 1, 0, 2);
    Stepper* pen = penSteppers[penIndex];
    int irPin = penIrPins[penIndex];

    int penQty = max(1, totalSheets);
    actualQty = penQty;
    for (int item = 1; item <= penQty; item++) {
      Serial.println("Pen requested. Moving to DROP position (180 deg)...");
      pen->step(1024);

      // --- WAIT FOR IR SENSOR DETECTION ---
      Serial.println("Waiting for pen drop detection...");
      unsigned long startTime = millis();
      bool detected = false;

      // Wait up to 5 seconds for the IR sensor (active LOW)
      while (millis() - startTime < 5000) {
        if (digitalRead(irPin) == LOW) { // IR beam broken
          detected = true;
          Serial.println(">>> PEN DROP DETECTED! <<<");
          break;
        }
      }

      delay(500); // Small pause for physical stability

      Serial.println("Returning to CATCH position at Top...");
      pen->step(-1024);
      stopStepper(penIndex);

      if (!detected) {
        Serial.println("ERROR: No pen detected by IR sensor!");
        CLOUD_SERIAL.println("ERR:Dispense Failed");
        showError("Dispense Failed");
        return; // Stop here, don't deduct credits
      }
    }

    Serial.println("Dispense Successful! Logging to cloud...");
    CLOUD_SERIAL.println("DONE:pen:" + currentRequestId + ":" + name + ":" + String(cost) + ":" + String(penQty));
  }

  credits -= cost;
  drawStatusScreen("Success", "Take it");
  tftUiShowSuccess("Take it");

  if (orderInProgress) {
    orderSummaryText += String(actualQty) + "x " + name + " - P" + String(cost, 2) + "\n";
    orderTotalCost += cost;
    cartDispenseIndex++;
    if (cartDispenseIndex < cartCount) {
      sendNextCartItem();
    } else {
      orderInProgress = false;
      cartCount = 0;
    }
    if (currentScreen == SCREEN_SUMMARY) drawSummaryScreen();
  }

  delay(3000);
  isProcessing = false;
  updateLCD();
  tftUiSetCredits();
}

void stopStepper(int penIndex) {
  for (int p = 0; p < 4; p++) digitalWrite(penStopPins[penIndex][p], LOW);
}

void returnChange() {
  if (credits <= 0) return;
  drawStatusScreen("Return", "P" + String((int)credits));
  servoChange.write(90); delay(2000); servoChange.write(0);
  credits = 0;
  updateLCD();
  tftUiSetCredits();
}

void updateLCD() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0,18);
  display.print("P");
  display.print((int)credits);

  display.setTextSize(1);
  display.setCursor(0,52);
  display.print("Ready to Serve");
  display.display();
}

void showError(String m) {
  drawStatusScreen("Error", m);
  tftUiShowError(m);
  if (orderInProgress) {
    orderInProgress = false;
    cartCount = 0;
    currentScreen = SCREEN_MAIN;
  }
  delay(2000);
  isProcessing = false;
  updateLCD();
  redrawCurrentScreen();
}

void drawStatusScreen(String headline, String message) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);

  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0,18);
  display.print(headline);

  display.setTextSize(1);
  display.setCursor(0,48);
  display.print(message);
  display.display();
}

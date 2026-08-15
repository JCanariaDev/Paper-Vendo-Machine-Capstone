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
  Install via Library Manager if missing:
    - Adafruit ILI9341
    - Adafruit GFX Library (dependency)
    - XPT2046_Touchscreen (by Paul Stoffregen)
*/

// =============================================================
// ARDUINO MEGA 2560 — COMPLETE PIN ASSIGNMENT REFERENCE
// =============================================================
//
// ── DIGITAL I/O ──────────────────────────────────────────────
//  D2   COIN_PIN            Coin acceptor pulse input (INPUT_PULLUP, INT0)
//  D3   penStepper1 IN1     28BYJ-48 pen slot 1, ULN2003 coil A
//  D4   penStepper1 IN2     28BYJ-48 pen slot 1, ULN2003 coil B
//  D7   PEN_IR_PIN          IR sensor pen slot 1 (INPUT_PULLUP, LOW = beam broken)
//  D8   LED_GREEN_PIN       Green LED — machine READY / AVAILABLE
//  D9   SERVO_CHANGE_PIN    Change-dispense servo signal
//  D10  SERVO_PEN_PIN       Pen-ejection servo signal
//  D11  penStepper1 IN3     28BYJ-48 pen slot 1, ULN2003 coil C
//  D12  penStepper1 IN4     28BYJ-48 pen slot 1, ULN2003 coil D
//  D13  LED_BLUE_PIN        Blue LED  — machine IDLE / IN USE (busy)
//  D14  CHANGE_HOPPER_MOTOR_PIN  Relay IN controlling coin hopper motor
//  D15  CHANGE_HOPPER_SENSOR_PIN Coin hopper exit IR sensor (INPUT_PULLUP)
//  D16  COIN_INHIBIT_PIN    Coin acceptor INHIBIT line (OUTPUT, active HIGH)
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
//  D32  PAPER_STEP_PINS[0]  Paper channel 1 STEP (A4988/TMC2209)
//  D33  PAPER_DIR_PINS[0]   Paper channel 1 DIR
//  D34  PAPER_STEP_PINS[1]  Paper channel 2 STEP
//  D35  PAPER_DIR_PINS[1]   Paper channel 2 DIR
//  D36  PAPER_STEP_PINS[2]  Paper channel 3 STEP
//  D37  PAPER_DIR_PINS[2]   Paper channel 3 DIR
//  D38  PAPER_STEP_PINS[3]  Paper channel 4 STEP
//  D39  PAPER_DIR_PINS[3]   Paper channel 4 DIR
//  D40  PAPER_ENABLE_PIN    Common ENABLE for all 4 paper stepper drivers (active LOW)
//  D41  PAPER_EXIT_PINS[0]  Paper channel 1 exit IR sensor (INPUT_PULLUP)
//  D42  PAPER_EXIT_PINS[1]  Paper channel 2 exit IR sensor (INPUT_PULLUP)
//  D43  PAPER_EXIT_PINS[2]  Paper channel 3 exit IR sensor (INPUT_PULLUP)
//  D44  PAPER_EXIT_PINS[3]  Paper channel 4 exit IR sensor (INPUT_PULLUP)
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
//       → Momentary press pulls to GND, triggers AVR watchdog full reboot
//  A9   SW_RESET_BTN_PIN    Software-reset push button (INPUT_PULLUP)
//       → Momentary press pulls to GND, calls softResetMachineState()
//
// ── I²C BUS (Wire) ───────────────────────────────────────────
//  D20  SDA    SH1106G OLED 128×64 (address 0x3C)
//  D21  SCL    SH1106G OLED 128×64
//
// ── HARDWARE SERIAL ──────────────────────────────────────────
//  D0 / D1  Serial0 (USB / Serial Monitor at 115200 baud)
//  D18 / D19 Serial1 (CLOUD_SERIAL to ESP32 at 9600 baud via LLC)
//            D18 = TX1 → ESP32 RX2 (Pin 16)
//            D19 = RX1 ← ESP32 TX2 (Pin 17)
//
// ── INDICATOR LEGEND ─────────────────────────────────────────
//  GREEN (D8)  — Machine READY / AVAILABLE  → safe to insert coins
//  BLUE  (D13) — Machine IDLE / IN USE      → booting, connecting, dispensing
//  RED   (D45) — Machine ERROR              → Wi-Fi lost, dispense failed
//  BUZZER (D46) — Passive 2-pin buzzer
//    READY  tone: two short chirps  (1800 Hz × 80 ms, 60 ms gap, repeat)
//    ACTIVE tone: one soft beep     (1100 Hz × 120 ms)
//    ERROR  tone: long low buzz     ( 350 Hz × 500 ms)
//
// ── RESET BUTTONS ────────────────────────────────────────────
//  SW RESET (A9):  Software reset — clears credits/cart/order, returns to
//                  IDLE, notifies ESP32. Does NOT reboot the MCU.
//  HW RESET (A8):  Hardware reset — triggers AVR watchdog for a full
//                  board reboot (equivalent to pressing the RESET pin).
// =============================================================

// --- PINS (existing) ---
const int COIN_PIN = 2;
const int COIN_INHIBIT_PIN = 16; // added: coin acceptor inhibit input, active HIGH by default
const bool COIN_INHIBIT_ACTIVE_HIGH = true;
// Three machine-state LEDs and one two-wire passive buzzer.
// GREEN = ready/available | BLUE = idle/in-use (busy) | RED = error
const int LED_GREEN_PIN = 8;
const int LED_BLUE_PIN = 13;
const int LED_RED_PIN = 45;
const int BUZZER_PIN = 46;

// --- RESET BUTTONS (Active-LOW: Pin -> Button -> GND) ---
// Wired to dedicated analog header pins A8/A9 to keep D18/D19 free for Serial1 (ESP32).
const int HW_RESET_BTN_PIN = A8; // A8 — full board reboot via watchdog
const int SW_RESET_BTN_PIN = A9; // A9 — software state reset
const int PEN_IR_PIN = 7;   // pen slot 1 IR sensor
const int PEN_IR_PIN2 = 30; // pen slot 2 IR sensor
const int PEN_IR_PIN3 = 31; // pen slot 3 IR sensor
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;
// Pen stepper pins: slot 1 = 3,4,11,12; slot 2 = 22,23,24,25;
// slot 3 = 26,27,28,29.

// --- ADDED: four physical paper channels and verified change hopper ---
// These pins do not replace any existing pen, TFT, coin, or Mega<->ESP wiring.
// Each paper channel needs a STEP/DIR driver and a normally-HIGH exit sensor.
const int PAPER_STEP_PINS[4] = { 32, 34, 36, 38 };
const int PAPER_DIR_PINS[4] = { 33, 35, 37, 39 };
const int PAPER_EXIT_PINS[4] = { 41, 42, 43, 44 };
const int PAPER_ENABLE_PIN = 40; // common ENABLE on the four stepper drivers (active LOW)
const int CHANGE_HOPPER_MOTOR_PIN = 14;
const int CHANGE_HOPPER_SENSOR_PIN = 15; // active LOW when a coin passes
const unsigned long PAPER_SHEET_TIMEOUT_MS = 12000;
const unsigned long CHANGE_COIN_TIMEOUT_MS = 5000;
const unsigned long PEN_SENSOR_TIMEOUT_MS = 5000;
const unsigned long HOPPER_MANUAL_MAX_MS = 10000;

// The tested SRD-05V relay module turns the hopper on when its IN terminal
// receives HIGH.  Change only this constant to LOW if your physical relay
// proves to be active-low (for example, it runs while the Mega is booting).
const int HOPPER_RELAY_ON = HIGH;
const int HOPPER_RELAY_OFF = (HOPPER_RELAY_ON == HIGH) ? LOW : HIGH;

// --- PINS (touchscreen) ---
// SPI bus (52/51/50) is fixed hardware SPI on the Mega - not redefinable.
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

// --- PEN STEPPER CONFIG ---
const int stepsPerRevolution = 2048;
// Keep this pin order matched to the ULN2003 wiring.
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

#if defined(HAVE_HWSERIAL1)
#define CLOUD_SERIAL Serial1
#else
#define CLOUD_SERIAL Serial
#endif

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Servo servoChange, servoPen;

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
// The TFT is rotated 180 degrees below. Reverse both calibrated touch axes
// so a physical tap still activates the item visibly under the finger.
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

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

// --- RESET BUTTON DEBOUNCE TIMERS ---
unsigned long hwResetDebounceUntil = 0;
unsigned long swResetDebounceUntil = 0;

// -----------------------------------------------------------------
// setMachineIndicator(state, sound)
//   GREEN (D8)  → INDICATOR_READY   : machine available, safe to insert coins
//   BLUE  (D13) → INDICATOR_ACTIVE  : booting / connecting / dispensing (busy)
//   RED   (D45) → INDICATOR_ERROR   : Wi-Fi lost, dispense failed, etc.
//
// Buzzer patterns (passive buzzer on D46, driven by tone()):
//   READY  — two short chirps: 1800 Hz × 80 ms, 60 ms silence, 1800 Hz × 80 ms
//   ACTIVE — one soft medium beep: 1100 Hz × 120 ms
//   ERROR  — long descending buzz: 350 Hz × 500 ms
// -----------------------------------------------------------------
void setMachineIndicator(IndicatorState state, bool sound = false) {
  indicatorState = state;
  digitalWrite(LED_GREEN_PIN, state == INDICATOR_READY  ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN,  state == INDICATOR_ACTIVE ? HIGH : LOW);
  digitalWrite(LED_RED_PIN,   state == INDICATOR_ERROR  ? HIGH : LOW);

  if (!sound) return;

  switch (state) {
    case INDICATOR_READY:
      // Two short chirps — "ready to serve" confirmation
      tone(BUZZER_PIN, 1800, 80);
      delay(140); // 80 ms tone + 60 ms gap
      tone(BUZZER_PIN, 1800, 80);
      break;

    case INDICATOR_ACTIVE:
      // One soft beep — machine acknowledges an action (busy/dispensing)
      tone(BUZZER_PIN, 1100, 120);
      break;

    case INDICATOR_ERROR:
      // Long low buzz — draws attention to an error condition
      tone(BUZZER_PIN, 350, 500);
      break;
  }
}


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

const int PAPER_COUNT = 4;
CatalogItem paperCatalog[PAPER_COUNT] = {
  {1, "1/4",       1.00},
  {2, "Crosswise", 1.00},
  {3, "Lengthwise",1.00},
  {4, "Whole",     1.00}
};

const int BALLPEN_COUNT = 3;
CatalogItem ballpenCatalog[BALLPEN_COUNT] = {
  {1, "Black Ballpen", 5.00},
  {2, "Blue Ballpen",  5.00},
  {3, "Red Ballpen",   5.00}
};

const int MAX_CATALOG_ROWS = 4; // must be >= max(PAPER_COUNT, BALLPEN_COUNT)
int pendingQty[MAX_CATALOG_ROWS];
String activeCatalogType = "paper"; // protocol values are "paper" and "pen"

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

// Green means a customer can safely start a transaction. Blue is used while
// booting/connecting or dispensing; red means Wi-Fi is unavailable or an
// operation has failed. Keep this decision in one place so an error does not
// incorrectly return to green while the ESP32 is still disconnected.
void refreshMachineAvailability(bool sound = false) {
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

// Keep this display estimate aligned with the default SQL seed.  The server
// still validates the real price at reservation time, so an administrator's
// later database price change cannot undercharge a transaction.
float catalogDisplayPrice(int index) {
  if (activeCatalogType == "paper") {
    return selectedPaperBrand == "Standard" ? 2.00 : 1.00;
  }
  return ballpenCatalog[index].price;
}

void setCoinAcceptance(bool allowed) {
  int level = allowed
    ? (COIN_INHIBIT_ACTIVE_HIGH ? LOW : HIGH)
    : (COIN_INHIBIT_ACTIVE_HIGH ? HIGH : LOW);
  digitalWrite(COIN_INHIBIT_PIN, level);
}

// ================= CATALOG LAYOUT =================
// Rows adapt to however many items are in the active catalog (3 for
// ballpen, 4 for paper) instead of a fixed size - both drawCatalogScreen()
// and handleCatalogTouch() call these same functions so the drawn
// buttons and the tappable zones can never drift apart.
const int CATALOG_TOP = 58;
const int CATALOG_BOTTOM = 268; // just above the ADD/CANCEL row at y=275

int catalogRowHeight(int count) {
  return (CATALOG_BOTTOM - CATALOG_TOP) / count;
}

int catalogRowY(int i, int count) {
  return CATALOG_TOP + i * catalogRowHeight(count);
}

int catalogButtonSize(int count) {
  return catalogRowHeight(count) - 8; // small gap between rows
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
  } else if (wifiStatus == WIFI_STATUS_CONNECTING) {
    // The spinner glyph itself is drawn/updated by drawWifiSpinnerFrame(),
    // called periodically from loop() - just make sure this area starts
    // clean and the animation restarts from frame 0 on a fresh draw.
    tft.fillRect(0, 190, tft.width(), 20, COL_BLACK);
    spinnerFrame = 0;
  }
}

void drawMainScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Paper and Ballpen", tft.width() / 2, 45);
  printCentered("Vending Machine", tft.width() / 2, 65);

  if (!uiWifiConnected) {
    tft.fillRect(0, 72, tft.width(), 18, COL_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(1);
    if (wifiStatus == WIFI_STATUS_NOT_FOUND) {
      printCentered("WiFi can't be detected", tft.width() / 2, 72 + 9);
    } else if (wifiStatus == WIFI_STATUS_CONNECTING) {
      printCentered("Connecting to WiFi...", tft.width() / 2, 72 + 9);
    } else {
      printCentered("Connect to WiFi first", tft.width() / 2, 72 + 9);
    }
  }

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
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

  // VIEW CART - deliberately available even without WiFi, since it only
  // shows what's already in the cart locally, no server/order action.
  tft.fillRoundRect(20, 285, 200, 30, 8, COL_GREY);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(1);
  String cartLabel = cartCount > 0 ? ("VIEW CART (" + String(cartCount) + ")") : "VIEW CART (empty)";
  printCentered(cartLabel.c_str(), tft.width() / 2, 285 + 15);
}

// A brand is a logical product choice.  The selected brand/size is mapped by
// product id to one of the four physical paper feeders in the database; the
// database is the source of truth for that mapping and stock reservation.
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

  // Rows are much taller than before (70px, buttons filling 60 of that)
  // so the +/- targets are large and forgiving, instead of the old
  // 30x40 buttons + a separate 24x24 checkbox that were easy to miss.
  // Selection is now just "qty > 0" - shown as a green row border -
  // there's no separate checkbox tap to land wrong anymore.
  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int btnSize = catalogButtonSize(count);
    bool selected = pendingQty[i] > 0;

    if (selected) {
      tft.drawRoundRect(4, rowY, 232, btnSize + 8, 8, COL_GREEN);
      tft.drawRoundRect(5, rowY + 1, 230, btnSize + 6, 8, COL_GREEN);
    }

    // "-" button, left side
    tft.fillRoundRect(8, rowY + 4, btnSize, btnSize, 8, COL_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(btnSize >= 46 ? 3 : 2);
    printCentered("-", 8 + btnSize / 2, rowY + 4 + btnSize / 2);

    // "+" button, right side (right-aligned so it scales with btnSize)
    int plusX = 232 - btnSize;
    tft.fillRoundRect(plusX, rowY + 4, btnSize, btnSize, 8, COL_GREEN);
    printCentered("+", plusX + btnSize / 2, rowY + 4 + btnSize / 2);

    // Middle info column: name, price, and current qty
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(1);
    tft.setCursor(72, rowY + 6);
    tft.print(catalog[i].name);
    tft.setCursor(72, rowY + 20);
    tft.print("P" + String(catalogDisplayPrice(i), 2));

    char qtyBuf[16];
    sprintf(qtyBuf, "Qty: %d", pendingQty[i]);
    tft.setTextSize(btnSize >= 46 ? 2 : 1);
    tft.setCursor(72, rowY + (btnSize >= 46 ? 34 : 32));
    tft.print(qtyBuf);
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

// ================= CART LAYOUT =================
// Same "shared helper functions between drawing and touch" pattern used
// for the catalog screen - drawCartScreen() and handleCartTouch() call
// these so the drawn remove buttons and the tappable zones can't drift
// apart from each other.
const int CART_TOP = 58;
const int CART_BOTTOM = 248; // leaves room for the Total line + BACK button

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

      // Remove "X" button, right side of the row
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
    case SCREEN_IDLE:    drawIdleScreen();    break;
    case SCREEN_MAIN:    drawMainScreen();    break;
    case SCREEN_PAPER_BRAND: drawPaperBrandScreen(); break;
    case SCREEN_CATALOG: drawCatalogScreen(); break;
    case SCREEN_CART:    drawCartScreen();    break;
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

void startOrder() {
  // A customer transaction must never inherit a manual hopper test state.
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  orderInProgress = true;
  setMachineIndicator(INDICATOR_ACTIVE, true);
  setCoinAcceptance(false); // freeze the credit snapshot during reservation/change/dispense
  orderSummaryText = "";
  orderTotalCost = 0;
  activeTransactionId = "";
  activeChangeCents = 0;
  currentScreen = SCREEN_SUMMARY;
  drawSummaryScreen();

  // Reserve the complete cart before any physical action.  The ESP32 turns
  // this compact message into JSON for the atomic Supabase RPC.
  String encodedLines = "";
  for (int i = 0; i < cartCount; i++) {
    if (encodedLines.length()) encodedLines += ';';
    encodedLines += cart[i].type + "," + String(cart[i].id) + "," + String(cart[i].qty);
  }
  CLOUD_SERIAL.println("RESERVE:" + String((unsigned long)credits * 100UL) + ":" + encodedLines);
}

// ================= TOUCH HANDLERS =================

void handleMainTouch(int x, int y) {
  // VIEW CART is deliberately checked before the WiFi gate below - it's
  // just showing what's already in the cart locally, no order/network
  // action, so it should work even while offline.
  // Measured after the 180-degree display/touch rotation.
  if (x >= 20 && x <= 230 && y >= 320 && y <= 340) {
    currentScreen = SCREEN_CART;
    drawCartScreen();
    return;
  }

  if (!uiWifiConnected) {
    tftUiShowError("Connect to WiFi first");
    return;
  }

  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    activeCatalogType = "paper";
    resetPendingSelections();
    currentScreen = SCREEN_PAPER_BRAND;
    drawPaperBrandScreen();
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

  // Uses the same catalogRowY/catalogButtonSize helpers as drawCatalogScreen()
  // so tappable zones exactly match whatever was actually drawn.
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
      // What the total would be if this tap goes through: whatever's
      // already committed to the cart (from any category) + everything
      // currently pending in this catalog view + one more of this item.
      // Blocks the + button once that would exceed inserted credits,
      // instead of letting it silently rack up more than was paid.
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

  // Measured after the 180-degree display/touch rotation.
  if (x >= 20 && x <= 120 && y >= 270 && y <= 350) { // ADD
    for (int i = 0; i < count; i++) {
      if (pendingQty[i] > 0) {
        int productId = catalog[i].id;
        String productName = catalog[i].name;
        if (activeCatalogType == "paper") {
          // Seeded ids 1..4 are Budget and 5..8 are Standard.  This is only
          // a stable UI lookup; price and sheets-per-unit are revalidated by
          // machine_reserve_transaction before any change or dispensing.
          if (selectedPaperBrand == "Standard") productId += 4;
          productName = selectedPaperBrand + " " + productName;
        }
        addToCart(activeCatalogType, productId, productName.c_str(), catalogDisplayPrice(i), pendingQty[i]);
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

void handleSummaryTouch(int x, int y) {
  // Change is released automatically, and only before PLAN is received.
  // There is intentionally no manual "D"/touch release action in production.
  (void)x; (void)y;
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
        drawCartScreen(); // stay on this screen so more items can be removed
        return;
      }
    }
  }

  // Measured after the 180-degree display/touch rotation.
  if (x >= 20 && x <= 230 && y >= 320 && y <= 340) { // BACK
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
  }
}

// ================= TFT UI ENTRY POINTS =================

void tftUiBegin() {
  tft.begin();
  tft.setRotation(2); // 180-degree TFT rotation; keeps the 240x320 layout
  diagTouchOk = ts.begin();
  // Raw touch coordinates are calibrated and inverted manually in tftUiLoop().
  // Keep this at 0 to avoid applying a second rotation in the library.
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

// Use int here because the Arduino IDE's auto-prototype generator places
// function prototypes before enum declarations in some builds.
void tftUiSetWifiStatus(int status) {
  bool changed = wifiStatus != status;
  wifiStatus = (WifiStatus)status;
  uiWifiConnected = (status == WIFI_STATUS_CONNECTED);
  if (!orderInProgress) refreshMachineAvailability(changed);
  // Idle and main screens both carry WiFi-dependent messaging, so give
  // those a full redraw when the status flips - not just the status bar -
  // so the spinner/"can't be detected" message appears/disappears
  // immediately rather than only updating on the next unrelated change.
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
  // Shifted up from the very bottom so this doesn't get instantly
  // painted over by the on-screen touch-debug readout (rev 11), which
  // occupies the last 12px of the screen.
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
    case SCREEN_MAIN:    handleMainTouch(x, y);    break;
    case SCREEN_PAPER_BRAND: handlePaperBrandTouch(x, y); break;
    case SCREEN_CATALOG: handleCatalogTouch(x, y); break;
    case SCREEN_CART:    handleCartTouch(x, y);    break;
    case SCREEN_SUMMARY: handleSummaryTouch(x, y); break;
    default: break; // SCREEN_IDLE has nothing to tap
  }

  // Touch coordinates remain available in the USB Serial Monitor only.
  // Do not draw them over the customer-facing TFT interface.
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
  Serial.println("SKIPPED - readback command isn't safe on this clone board (can break future draws). Check visually: does the screen show your UI correctly?");

  Serial.print("Touchscreen (XPT2046)..... ");
  Serial.println(diagTouchOk ? "OK" : "FAIL - ts.begin() returned false");

  Serial.print("Coin acceptor pin (D2).... ");
  Serial.println("configured (INPUT_PULLUP + interrupt) - insert a coin to confirm live");

  Serial.println("Servos (D9 change / D10 pen) - attached, no electrical feedback available");

  Serial.print("WiFi (from ESP32 via CLOUD_SERIAL)... ");
  switch (wifiStatus) {
    case WIFI_STATUS_CONNECTED:  Serial.println("OK - last reported CONNECTED"); break;
    case WIFI_STATUS_CONNECTING: Serial.println("Attempting to connect - target network was seen in scan"); break;
    case WIFI_STATUS_NOT_FOUND:  Serial.println("Target network NOT seen in ESP32's scan"); break;
    default:                     Serial.println("Not connected yet - waiting for a status message from the ESP32"); break;
  }

  Serial.println("--- Pen IR sensors (active LOW = beam broken) ---");
  for (int i = 0; i < 3; i++) {
    Serial.print("  Slot "); Serial.print(i + 1); Serial.print(" (D");
    Serial.print(penIrPins[i]); Serial.print("): ");
    Serial.println(digitalRead(penIrPins[i]) == HIGH ? "OK - beam clear" : "WARNING - LOW at idle; check IR alignment");
  }

  Serial.println("==================================");
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
  Serial.print("Hopper relay D14: ");
  Serial.println(digitalRead(CHANGE_HOPPER_MOTOR_PIN) == HOPPER_RELAY_ON ? "ON" : "OFF");
  Serial.print("Hopper sensor D15: ");
  Serial.println(digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW ? "LOW / blocked" : "HIGH / clear");
  Serial.print("HW Reset btn A8: ");
  Serial.println(digitalRead(HW_RESET_BTN_PIN) == LOW ? "PRESSED (shorted to GND)" : "OK - idle HIGH");
  Serial.print("SW Reset btn A9: ");
  Serial.println(digitalRead(SW_RESET_BTN_PIN) == LOW ? "PRESSED (shorted to GND)" : "OK - idle HIGH");
  Serial.println("Commands: STATUS | SOFT_RESET | ESP_RESET | RESET:ALL | HOPPER 1 | HOPPER ON | HOPPER OFF");
}

void softResetMachineState() {
  Serial.println("SOFT RESET: returning machine logic to idle state.");

  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  digitalWrite(PAPER_ENABLE_PIN, HIGH);
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
  Serial.println("SOFT RESET: done.");
}

// ================= EXISTING MACHINE LOGIC =================

void setup() {
  Serial.begin(115200);
  #if defined(HAVE_HWSERIAL1)
  CLOUD_SERIAL.begin(9600); // To ESP32
  #endif
  Serial.println("--- SYSTEM STARTING ---");

  for (int i = 0; i < 3; i++) penSteppers[i]->setSpeed(10);
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

  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  // Start blue until the ESP32 confirms that the database connection path is
  // available. The Wi-Fi status handler changes it to green/red afterward.
  setMachineIndicator(INDICATOR_ACTIVE, true);

  // --- RESET BUTTONS ---
  // Both wired active-LOW: pin → button → GND. INPUT_PULLUP keeps line HIGH at rest.
  pinMode(HW_RESET_BTN_PIN, INPUT_PULLUP); // A8
  pinMode(SW_RESET_BTN_PIN, INPUT_PULLUP); // A9
  Serial.println("Reset buttons configured: HW=A8, SW=A9 (INPUT_PULLUP).");

  pinMode(COIN_PIN, INPUT_PULLUP);
  pinMode(COIN_INHIBIT_PIN, OUTPUT);
  setCoinAcceptance(true);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);
  pinMode(PEN_IR_PIN, INPUT_PULLUP);
  pinMode(PEN_IR_PIN2, INPUT_PULLUP);
  pinMode(PEN_IR_PIN3, INPUT_PULLUP);
  for (int i = 0; i < 4; i++) {
    pinMode(PAPER_STEP_PINS[i], OUTPUT);
    pinMode(PAPER_DIR_PINS[i], OUTPUT);
    pinMode(PAPER_EXIT_PINS[i], INPUT_PULLUP);
    digitalWrite(PAPER_STEP_PINS[i], LOW);
  }
  pinMode(PAPER_ENABLE_PIN, OUTPUT);
  digitalWrite(PAPER_ENABLE_PIN, HIGH); // drivers disabled until a paper plan arrives
  pinMode(CHANGE_HOPPER_MOTOR_PIN, OUTPUT);
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  pinMode(CHANGE_HOPPER_SENSOR_PIN, INPUT_PULLUP);

  servoChange.attach(SERVO_CHANGE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  servoChange.write(0);
  servoPen.write(0);
  Serial.println("Servos configured.");

  Serial.println("Machine Ready!");
  updateLCD();
  CLOUD_SERIAL.println("CREDIT:" + String((unsigned int)credits));
  CLOUD_SERIAL.println("STATUS?");

  runDiagnostics(); // auto-run once at boot; type DIAG later to re-run
}

void coinInterrupt() {
  if (orderInProgress) return; // guard against a pulse already in flight when inhibit is asserted
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

  // ── HARDWARE RESET BUTTON (A8) ──────────────────────────────────────────
  // Pressing A8 pulls pin LOW. We debounce for 80 ms, then trigger the AVR
  // watchdog (15 ms timeout) for a full board reboot.
  if (digitalRead(HW_RESET_BTN_PIN) == LOW && now >= hwResetDebounceUntil) {
    hwResetDebounceUntil = now + 1000;
    Serial.println("HW RESET BUTTON (A8): triggering watchdog reboot...");
    Serial.flush();
    setMachineIndicator(INDICATOR_ERROR, false);
    tone(BUZZER_PIN, 500, 200);
    delay(200);
    noInterrupts();
    wdt_enable(WDTO_15MS); // arm watchdog — board resets in ~15 ms
    while (true) {}        // spin until WDT fires
  }

  // ── SOFTWARE RESET BUTTON (A9) ──────────────────────────────────────────
  // Pressing A9 pulls pin LOW. Calls softResetMachineState() to clear orders,
  // reset credits, and return to IDLE without restarting the MCU.
  if (digitalRead(SW_RESET_BTN_PIN) == LOW && now >= swResetDebounceUntil) {
    swResetDebounceUntil = now + 500;
    Serial.println("SW RESET BUTTON (A9): performing software state reset...");
    softResetMachineState();
  }

  tftUiLoop();

  // Animate the WiFi-connecting spinner, independent of any full-screen
  // redraw, so it doesn't flicker/rebuild the whole screen every tick.
  if (currentScreen == SCREEN_IDLE && wifiStatus == WIFI_STATUS_CONNECTING) {
    if (millis() - lastSpinnerUpdate > 200) {
      lastSpinnerUpdate = millis();
      drawWifiSpinnerFrame();
    }
  }

  // A manual hopper test must never leave the relay powered indefinitely.
  if (hopperManualRunning && millis() - hopperManualStartedAt >= HOPPER_MANUAL_MAX_MS) {
    digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
    hopperManualRunning = false;
    Serial.println("Hopper manual safety timeout: OFF");
  }

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

  // --- Hardware diagnostics, typed into the USB Serial Monitor ---
  // These are intentionally separate from CLOUD_SERIAL, so a bench test
  // cannot send a false transaction message to the ESP32/database.
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "DIAG") {
      runDiagnostics();
    } else if (cmd == "STATUS") {
      printHardwareStatus();
    } else if (cmd == "TEST:GREEN") {
      setMachineIndicator(INDICATOR_READY, true);
      Serial.println("TEST: green LED and ready tone.");
    } else if (cmd == "TEST:BLUE") {
      setMachineIndicator(INDICATOR_ACTIVE, true);
      Serial.println("TEST: blue LED and active tone.");
    } else if (cmd == "TEST:RED") {
      setMachineIndicator(INDICATOR_ERROR, true);
      Serial.println("TEST: red LED and error tone.");
    } else if (cmd == "TEST:BUZZER") {
      tone(BUZZER_PIN, 1500, 300);
      Serial.println("TEST: buzzer tone.");
    } else if (cmd == "SOFT_RESET") {
      softResetMachineState();
    } else if (cmd == "ESP_RESET") {
      CLOUD_SERIAL.println("ESP_RESET");
      Serial.println("Requested ESP32 software restart.");
    } else if (cmd == "RESET:ALL") {
      CLOUD_SERIAL.println("ESP_RESET");
      softResetMachineState();
    } else if (cmd == "HOPPER ON") {
      if (orderInProgress) {
        Serial.println("HOPPER ABORT: an order is active.");
      } else {
        digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
        hopperManualRunning = true;
        hopperManualStartedAt = millis();
        Serial.println("Hopper: ON (safety stop in 10 seconds; send HOPPER OFF to stop now).");
      }
    } else if (cmd == "HOPPER OFF") {
      digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
      hopperManualRunning = false;
      Serial.println("Hopper: OFF");
    } else if (cmd.startsWith("HOPPER ")) {
      if (orderInProgress) {
        Serial.println("HOPPER ABORT: an order is active.");
      } else {
        int coins = cmd.substring(7).toInt();
        if (coins <= 0) {
          Serial.println("HOPPER FAIL: use HOPPER 1, HOPPER 2, etc.");
        } else {
          int result = releaseVerifiedChange(coins * 100);
          Serial.println(result == coins * 100 ? "HOPPER PASS: all coins sensor-confirmed." : "HOPPER FAIL: check hopper/sensor.");
        }
      }
    } else if (cmd == "DIAG:STATUS") {
      runDiagnostics();
      printHardwareStatus();
    } else if (cmd.startsWith("DIAG:PEN")) {
      int slot = cmd.substring(8).toInt(); // DIAG:PEN1/2/3
      if (orderInProgress) {
        Serial.println("DIAG ABORT: an order is active.");
      } else if (slot < 1 || slot > 3) {
        Serial.println("DIAG FAIL: use DIAG:PEN1, DIAG:PEN2, or DIAG:PEN3.");
      } else {
        bool passed = dispenseOnePen(slot);
        Serial.println(passed ? "PEN PASS: IR confirmed the item." : "PEN FAIL: sensor did not confirm an item.");
      }
    } else if (cmd.startsWith("DIAG:HOPPER")) {
      String value = cmd.substring(11);
      if (orderInProgress) {
        Serial.println("DIAG ABORT: an order is active.");
      } else if (value == "OFF") {
        digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
        hopperManualRunning = false;
        Serial.println("Hopper: OFF");
      } else {
        int coins = value.toInt(); // DIAG:HOPPER1, DIAG:HOPPER2, etc.
        if (coins <= 0) {
          Serial.println("DIAG FAIL: use DIAG:HOPPER1, DIAG:HOPPER2, etc.");
        } else {
          int result = releaseVerifiedChange(coins * 100);
          Serial.println(result == coins * 100 ? "HOPPER PASS: all coins sensor-confirmed." : "HOPPER FAIL: check hopper/sensor.");
        }
      }
    } else if (cmd.startsWith("DIAG:MOVE")) {
      int slot = cmd.substring(9).toInt(); // DIAG:MOVE1/2/3
      if (orderInProgress || slot < 1 || slot > 3) {
        Serial.println("DIAG ABORT: use an idle machine and DIAG:MOVE1/2/3.");
      } else {
        int idx = slot - 1;
        Serial.print("Jogging pen slot "); Serial.println(slot);
        penSteppers[idx]->step(200);
        delay(300);
        penSteppers[idx]->step(-200);
        stopStepper(idx);
        Serial.println("Done - did slot " + String(slot) + " visibly move?");
      }
    } else if (cmd.length()) {
      Serial.println("Commands: STATUS | SOFT_RESET | ESP_RESET | RESET:ALL | HOPPER 1 | HOPPER ON | HOPPER OFF | TEST:GREEN/BLUE/RED/BUZZER | DIAG | DIAG:PEN1..3 | DIAG:MOVE1..3");
      Serial.println("Hardware buttons: A8=HW_RESET (watchdog reboot) | A9=SW_RESET (state reset)");
    }
  }
}

bool dispenseOnePen(int channel) {
  int penIndex = channel - 1;
  if (penIndex < 0 || penIndex > 2) return false;
  Stepper* pen = penSteppers[penIndex];
  int irPin = penIrPins[penIndex];
  if (digitalRead(irPin) == LOW) {
    Serial.println("PEN ABORT: IR is LOW before dispense; clear/align the chute.");
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

bool dispenseOnePaper(int channel) {
  int index = channel - 1;
  if (index < 0 || index > 3) return false;
  const int sensor = PAPER_EXIT_PINS[index];
  const int stepPin = PAPER_STEP_PINS[index];
  digitalWrite(PAPER_DIR_PINS[index], HIGH); // reverse only if the installed feeder runs backwards
  digitalWrite(PAPER_ENABLE_PIN, LOW);

  // A blocked sensor at the start normally means a sheet was left in the
  // chute.  Do not count it as a newly dispensed sheet.
  unsigned long clearStartedAt = millis();
  while (digitalRead(sensor) == LOW && millis() - clearStartedAt < 1000) { }
  if (digitalRead(sensor) == LOW) {
    digitalWrite(PAPER_ENABLE_PIN, HIGH);
    return false;
  }

  bool seenSheet = false;
  unsigned long startedAt = millis();
  while (millis() - startedAt < PAPER_SHEET_TIMEOUT_MS) {
    digitalWrite(stepPin, HIGH); delayMicroseconds(700);
    digitalWrite(stepPin, LOW);  delayMicroseconds(700);
    if (digitalRead(sensor) == LOW) seenSheet = true;
    // Count only a complete clear -> blocked -> clear sensor cycle.
    if (seenSheet && digitalRead(sensor) == HIGH) {
      digitalWrite(PAPER_ENABLE_PIN, HIGH);
      return true;
    }
  }
  digitalWrite(PAPER_ENABLE_PIN, HIGH);
  return false;
}

void stopStepper(int penIndex) {
  for (int p = 0; p < 4; p++) digitalWrite(penStopPins[penIndex][p], LOW);
}

int releaseVerifiedChange(int changeCents) {
  if (changeCents == 0) return 0;
  // Initial release uses one physical P1 hopper.  The server reserves the
  // same denomination, so this routine must prove every requested coin via
  // the hopper's exit sensor before the machine is allowed to dispense goods.
  if (changeCents % 100 != 0) return -1;
  // The hopper exit sensor must be clear before the motor starts. Otherwise
  // an old blocked signal could be mistaken for a newly released P1 coin.
  if (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW) {
    Serial.println("HOPPER ABORT: exit sensor is LOW; clear/verify it first.");
    return -1;
  }
  const int expectedCoins = changeCents / 100;
  int countedCoins = 0;
  bool previousBlocked = false;
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
  unsigned long lastCoinAt = millis();
  while (countedCoins < expectedCoins && millis() - lastCoinAt < CHANGE_COIN_TIMEOUT_MS) {
    bool blocked = digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW;
    if (blocked && !previousBlocked) {
      countedCoins++;
      lastCoinAt = millis();
    }
    previousBlocked = blocked;
  }
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  return countedCoins == expectedCoins ? countedCoins * 100 : -1;
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
    for (int item = 0; item < expectedOutput; item++) {
      bool released = type == "paper" ? dispenseOnePaper(channel) : dispenseOnePen(channel);
      if (!released) break;
      actualOutput++;
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
  credits = 0; // change was confirmed before the ESP sent PLAN.
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
  else if (msg.startsWith("WIFI:")) {
    bool connected = msg.substring(5) == "1";
    // Debug visibility: if this line never prints, the Mega isn't
    // receiving anything from the ESP32 at all - check the physical
    // Serial1<->Serial2 wiring before assuming it's a code issue.
    Serial.println("Received WIFI status from ESP32: " + String(connected ? "CONNECTED" : "DISCONNECTED"));
    tftUiSetWifiConnected(connected);
  }
  else if (msg == "WIFISTATE:CONNECTING") {
    Serial.println("ESP32 found the target network - attempting to connect...");
    tftUiSetWifiStatus(WIFI_STATUS_CONNECTING);
  }
  else if (msg == "WIFISTATE:NOTFOUND") {
    Serial.println("ESP32 did not see the target WiFi network in its scan.");
    tftUiSetWifiStatus(WIFI_STATUS_NOT_FOUND);
  }
  else if (msg == "TEST_STEPPER_FWD") {
    penSteppers[0]->step(20);
    stopStepper(0);
  }
  else if (msg == "TEST_STEPPER_BACK") {
    penSteppers[0]->step(-20);
    stopStepper(0);
  }
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

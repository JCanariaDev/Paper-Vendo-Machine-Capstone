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
  - Manages ILI9341 Touch UI, SH1106 OLED, Coin Acceptor, Coin Hopper, and 1 Pen slot.
  - Communicates with ESP32 (Cloud Gateway) via Serial1 (Pins 18/19).
  - Communicates with Arduino Uno (Dedicated 2-Bay Paper Controller) via Serial2 (Pins 16/17).
  ==============================================================================
*/

// =============================================================
// ARDUINO MEGA 2560 — PIN ASSIGNMENTS (OPTION A ARCHITECTURE)
// =============================================================
//
// -- DIGITAL I/O ----------------------------------------------
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
//  D16  TX2 (Serial2)       -> Arduino Uno RX (Pin D0) at 5V logic
//  D17  RX2 (Serial2)       <- Arduino Uno TX (Pin D1) at 5V logic
//  D18  TX1 (Serial1)       -> ESP32 RX2 (via 3.3V logic level converter)
//  D19  RX1 (Serial1)       <- ESP32 TX2 (via 3.3V logic level converter)
//  D22-D29                 Former pen slots 2-3 stepper pins (unused in 1-slot layout)
//  D30-D31                 Former pen slots 2-3 IR pins (unused in 1-slot layout)
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
// -- ANALOG HEADER (Used as Digital Inputs) --------------------
//  A8   HW_RESET_BTN_PIN    Hardware-reset push button (INPUT_PULLUP)
//  A9   SW_RESET_BTN_PIN    Software-reset push button (INPUT_PULLUP)
//
// -- I²C BUS (Wire) -------------------------------------------
//  D20  SDA    SH1106G OLED 128×64 (address 0x3C)
//  D21  SCL    SH1106G OLED 128×64
// =============================================================

// --- PINS (existing) ---
const int COIN_PIN = 2;
const int COIN_INHIBIT_PIN = 6; // Pin D6: Drives Coin Acceptor Relay
// EXACT HARDWARE CALIBRATION:
// Pin D6 HIGH -> Relay LED OFF -> 12V ON (Coin Acceptor Powered ON)
// Pin D6 LOW  -> Relay LED ON  -> 12V CUT (Coin Acceptor Powered OFF / Rejects Coins)
int coinRelayOnLevel  = HIGH;   // HIGH = Power ON
int coinRelayOffLevel = LOW;    // LOW  = Power OFF (Cut at >= 30 credits)
const uint16_t MAX_CREDITS_ALLOWED = 30; // Maximum allowed credits (PHP 30 cap)
volatile unsigned long ignoreCoinPulsesUntil = 0; // Anti-glitch surge filter on relay switching
volatile bool coinAcceptorEnabled = true;         // Software gate for coin pulses
// Option A: Delayed relay cutoff to capture all pulses from last inserted coin
volatile bool pendingCoinAcceptorOff = false;     // Relay cut is queued, waiting for burst to finish
volatile unsigned long lastCoinBurstTime = 0;     // Timestamp of most recent valid coin pulse
const unsigned long COIN_BURST_SILENCE_MS = 350;  // Wait 350ms of silence before physically cutting relay

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

const int HOPPER_RELAY_ON  = LOW;  // LOW  = Relay LED ON  -> Motor ON
const int HOPPER_RELAY_OFF = HIGH; // HIGH = Relay LED OFF -> Motor OFF

// --- TFT & TOUCH PINS ---
#define TFT_CS   53
#define TFT_DC   48
#define TFT_RST  49
#define TOUCH_CS 47

// --- PEN STEPPERS ---
const int stepsPerRevolution = 2048;
Stepper penStepper1(stepsPerRevolution, 3, 11, 4, 12);
Stepper* penSteppers[1] = { &penStepper1 };
const int penStopPins[1][4] = {
  { 3, 4, 11, 12 }
};
const int penIrPins[1] = { PEN_IR_PIN };

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
String activeTrNumber = "";          // Human-readable TR Record Number (e.g. "TR-00001")
int activeChangeDueCents = 0;        // Total change owed to user
int activeChangePaidCents = 0;       // Total change physically released by hopper
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
void printCentered(const String &text, int cx, int cy);
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
void drawReceiptScreen();
void redrawCurrentScreen();
void handleMainTouch(int x, int y);
void handlePaperBrandTouch(int x, int y);
void handleCatalogTouch(int x, int y);
void handleCartTouch(int x, int y);
void handleReceiptTouch(int x, int y);
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
void removeFromCart(int index);
void addToCart(String type, int id, const char* name, float price, int qty);
void startOrder();
void tftUiBegin();
void tftUiSetCredits();
void tftUiSetWifiStatus(int status);
void tftUiSetWifiConnected(bool connected);
void drawWifiSpinnerFrame();
void parsePaperBay(String msg);
void parsePenBay(String msg);
void runDiagnostics();
void printHardwareStatus();
void executeDispensePlan(String message);
void beginReservedTransaction(String message);
void finishUiAfterTransaction(String message);

// ================= CATALOG =================
struct CatalogItem {
  int id;
  const char* name;
  float price;
  bool isPaperPresent; // Synced from Uno's L5290 sensors
};

const int PAPER_COUNT = 2;
CatalogItem paperCatalog[PAPER_COUNT] = {
  {1, "Bay 1 Paper",  1.00, true},
  {2, "Bay 2 Paper",  1.00, true}
};

const int BALLPEN_COUNT = 1;
CatalogItem ballpenCatalog[BALLPEN_COUNT] = {
  {1, "Pen Slot 1", 5.00, true}
};

// Dynamic name buffers — updated by ESP32 PAPER_BAY: / PEN_BAY: messages
char paperCatalogNames[PAPER_COUNT][32];
char ballpenCatalogNames[BALLPEN_COUNT][32];
int  ballpenCatalogStock[BALLPEN_COUNT]; // pen stock per bay (pieces)

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
enum UiScreen { SCREEN_IDLE, SCREEN_MAIN, SCREEN_PAPER_BRAND, SCREEN_CATALOG, SCREEN_CART, SCREEN_SUMMARY, SCREEN_RECEIPT };
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

// ================= DRAWING HELPERS =================
const int CATALOG_TOP = 58;
const int CATALOG_BOTTOM = 268;
const int CATALOG_MAX_ROW_HEIGHT = 70;
const int CATALOG_MAX_BUTTON_SIZE = 44;

int catalogRowHeight(int count) {
  if (count <= 0) return CATALOG_MAX_ROW_HEIGHT;
  return min((CATALOG_BOTTOM - CATALOG_TOP) / count, CATALOG_MAX_ROW_HEIGHT);
}

int catalogRowsStartY(int count) {
  int totalRowsHeight = catalogRowHeight(count) * count;
  int availableHeight = CATALOG_BOTTOM - CATALOG_TOP;
  return CATALOG_TOP + max(0, (availableHeight - totalRowsHeight) / 2);
}

int catalogRowY(int i, int count) {
  return catalogRowsStartY(count) + i * catalogRowHeight(count);
}

int catalogButtonSize(int count) {
  return max(34, min(catalogRowHeight(count) - 12, CATALOG_MAX_BUTTON_SIZE));
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

// ================= UNO SERIAL MESSAGES (L5290 PRESENCE & STATUS) =================
// ================= CATALOG SYNC FROM ESP32 =================
// Handles: PAPER_BAY:<bay>:<prod_id>:<presence>:<sheets>:<price_cents>:<name>
// Handles: PEN_BAY:<bay>:<prod_id>:<stock>:<price_cents>:<name>
// Sends dispense command to Arduino Uno and waits for sensor confirmation
// ================= DIAGNOSTICS =================
// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  CLOUD_SERIAL.begin(9600); // UART to ESP32 (Pins 18/19)
  UNO_SERIAL.begin(9600);   // UART to Arduino Uno (Pins 16/17)
  Serial.println("--- REVAMPED SMART PAPER VENDO FIRMWARE (OPTION A) STARTING ---");

  for (int i = 0; i < BALLPEN_COUNT; i++) penSteppers[i]->setSpeed(10);

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
  bool pendingOff = pendingCoinAcceptorOff;
  unsigned long burstTime = lastCoinBurstTime;
  coinPulseReceived = false;
  interrupts();

  if (pulseSnapshot) {
    Serial.println("Coin pulse detected on D2.");
  }

  // Option A: Fire deferred relay cutoff only after 350ms of coin pulse silence.
  // This ensures all remaining pulses of the last inserted multi-peso coin are credited
  // before we physically cut 12V power to the coin acceptor.
  if (pendingOff && (millis() - burstTime >= COIN_BURST_SILENCE_MS)) {
    noInterrupts();
    pendingCoinAcceptorOff = false;  // Clear the flag
    interrupts();
    setCoinAcceptance(false);
    Serial.println("MAX CREDIT CAP (P" + String(MAX_CREDITS_ALLOWED) + ") REACHED: Coin acceptor relay turned OFF (Power Cut). Final credits: P" + String((unsigned int)creditSnapshot));
    tftUiShowError("Max P30 credit reached");
  }

  if (creditSnapshot != lastCredits) {
    lastCredits = creditSnapshot;
    updateLCD();
    tftUiSetCredits();
    CLOUD_SERIAL.println("CREDIT:" + String((unsigned int)creditSnapshot));
    Serial.println("Credits inserted! Total: P" + String((unsigned int)creditSnapshot));

    // Re-enable coin acceptor after each credit update (no WiFi dependency)
    if (creditSnapshot < MAX_CREDITS_ALLOWED && !pendingOff && !orderInProgress && uiWifiConnected) {
      setCoinAcceptance(true);
    }
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
    else if (cmd == "COIN ON" || cmd == "ACCEPTOR ON") {
      setCoinAcceptance(true);
      Serial.print("MANUAL COIN ACCEPTOR: Power ON. Pin D6 level = ");
      Serial.println(digitalRead(COIN_INHIBIT_PIN) == HIGH ? "HIGH (5V)" : "LOW (0V)");
    } else if (cmd == "COIN OFF" || cmd == "ACCEPTOR OFF") {
      setCoinAcceptance(false);
      Serial.print("MANUAL COIN ACCEPTOR: Power OFF (Cut). Pin D6 level = ");
      Serial.println(digitalRead(COIN_INHIBIT_PIN) == HIGH ? "HIGH (5V)" : "LOW (0V)");
    } else if (cmd == "COIN INVERT") {
      int tmp = coinRelayOnLevel;
      coinRelayOnLevel = coinRelayOffLevel;
      coinRelayOffLevel = tmp;
      setCoinAcceptance(credits < MAX_CREDITS_ALLOWED);
      Serial.print("COIN RELAY POLARITY FLIPPED. ON level is now = ");
      Serial.println(coinRelayOnLevel == HIGH ? "HIGH (5V)" : "LOW (0V)");
    } else if (cmd == "COIN 1") {
      ignoreCoinPulsesUntil = millis() + 600;
      digitalWrite(COIN_INHIBIT_PIN, HIGH);
      Serial.println("DIRECT PIN D6 -> HIGH (5V)");
    } else if (cmd == "COIN 0") {
      ignoreCoinPulsesUntil = millis() + 600;
      digitalWrite(COIN_INHIBIT_PIN, LOW);
      Serial.println("DIRECT PIN D6 -> LOW (0V)");
    } else if (cmd == "HOPPER ON") {
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

// Releases verified change using Coin Hopper (Non-blocking fallback)

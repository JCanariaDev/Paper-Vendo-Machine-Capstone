/*
 * Paper Vendo Machine - Arduino Code
 * Handles: Coin acceptor, user buttons, LCD, and paper dispensing
 * Communicates with single ESP32 via SoftwareSerial.
 *
 * Sizes supported (Philippines HS): 
 *   1/4, crosswise, lengthwise, 1 whole
 *
 * Protocol with ESP32:
 *   -> REQ:BRAND_ID:SIZE_CODE
 *   <- DISP:SHEETS:COST
 *   -> DONE:BRAND_ID:SIZE_CODE:AMOUNT:SHEETS
 *   -> ERR:ERROR_MESSAGE (optional)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ------------------------------------------------
// PINS & CONFIG
// ------------------------------------------------
// Size selection buttons
#define BTN_SIZE_Q        2   // 1/4 sheet
#define BTN_SIZE_CROSS    3   // crosswise
#define BTN_SIZE_LENGTH   4   // lengthwise
#define BTN_SIZE_WHOLE    8   // 1 whole

// Coin Acceptor (P1 pulse)
#define COIN_PIN          5

// Servos per size (each controls one paper bin / separation mechanism)
Servo servoQ;
Servo servoCross;
Servo servoLength;
Servo servoWhole;

#define SERVO_Q_PIN       9
#define SERVO_CROSS_PIN   10
#define SERVO_LENGTH_PIN  11
#define SERVO_WHOLE_PIN   12

// LCD (I2C)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Serial to ESP32 (UNO uses SoftwareSerial)
SoftwareSerial espSerial(6, 7); // RX, TX

// ------------------------------------------------
// LOGIC CONFIG (Mapping between button, brand, and size)
// ------------------------------------------------
// NOTE: These brand IDs must exist in `paper_settings` table.
// Example mapping: all sizes use brand 1 by default.
// You can change these IDs to match specific pad/brand per bin.
const int SIZE_BUTTONS[4] = { BTN_SIZE_Q, BTN_SIZE_CROSS, BTN_SIZE_LENGTH, BTN_SIZE_WHOLE };
const int SIZE_BRAND_ID[4] = { 1, 1, 1, 1 };  // brand_id for each bin
const char* SIZE_CODES[4]  = { "1/4", "crosswise", "lengthwise", "1_whole" };

Servo* SIZE_SERVOS[4] = { &servoQ, &servoCross, &servoLength, &servoWhole };

// ------------------------------------------------
// RUNTIME STATE
// ------------------------------------------------
volatile int credits = 0;          // Available pesos
int pendingIndex = -1;             // Which size index is currently requested
bool waitingForDispense = false;   // True after sending REQ until DONE

// ------------------------------------------------
// FORWARD DECLARATIONS
// ------------------------------------------------
void coinInterrupt();
void updateLCD();
void handleSizeButton(int index);
void processEspMessage(String msg);
void performDispense(int index, int sheets, int cost);
void runSeparationCycle(Servo* s, int count);
void resetServos();

// ------------------------------------------------
// SETUP
// ------------------------------------------------
void setup() {
  Serial.begin(9600);      // Debugging
  espSerial.begin(9600);   // To ESP32

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Paper Vendo");
  lcd.setCursor(0, 1);
  lcd.print("Ready...");

  // Inputs
  for (int i = 0; i < 4; i++) {
    pinMode(SIZE_BUTTONS[i], INPUT_PULLUP);
  }
  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, RISING);

  // Servos
  servoQ.attach(SERVO_Q_PIN);
  servoCross.attach(SERVO_CROSS_PIN);
  servoLength.attach(SERVO_LENGTH_PIN);
  servoWhole.attach(SERVO_WHOLE_PIN);
  resetServos();

  delay(1000);
  updateLCD();
}

// ------------------------------------------------
// MAIN LOOP
// ------------------------------------------------
void loop() {
  // 1. Check size buttons
  for (int i = 0; i < 4; i++) {
    if (digitalRead(SIZE_BUTTONS[i]) == LOW) {
      handleSizeButton(i);
      delay(300); // basic debounce
    }
  }

  // 2. Check ESP32 responses
  if (espSerial.available()) {
    String resp = espSerial.readStringUntil('\n');
    resp.trim();
    if (resp.length() > 0) {
      Serial.println("ESP32: " + resp);
      processEspMessage(resp);
    }
  }
}

// ------------------------------------------------
// INTERRUPTS & UI
// ------------------------------------------------
void coinInterrupt() {
  static unsigned long lastPulse = 0;
  unsigned long now = millis();

  if (now - lastPulse > 50) { // debounce ~50ms
    credits++;   // 1 peso per pulse
    lastPulse = now;
    updateLCD();
  }
}

void updateLCD() {
  lcd.setCursor(0, 0);
  lcd.print("Credits: P");
  lcd.print(credits);
  lcd.print("   "); // clear tail

  lcd.setCursor(0, 1);
  lcd.print("Select size -> ");
}

// ------------------------------------------------
// BUTTON HANDLING
// ------------------------------------------------
void handleSizeButton(int index) {
  if (waitingForDispense) {
    // Prevent sending another request while busy
    lcd.setCursor(0, 1);
    lcd.print("Please wait... ");
    return;
  }

  if (credits < 1) { // assume minimum 1 peso
    lcd.setCursor(0, 0);
    lcd.print("Insert P1 coin ");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    delay(1200);
    updateLCD();
    return;
  }

  int brandId = SIZE_BRAND_ID[index];
  const char* sizeCode = SIZE_CODES[index];

  lcd.setCursor(0, 0);
  lcd.print("Checking ");
  lcd.print(sizeCode);
  lcd.print("    ");

  // Send REQ:BRAND_ID:SIZE_CODE\n
  espSerial.print("REQ:");
  espSerial.print(brandId);
  espSerial.print(":");
  espSerial.println(sizeCode);

  pendingIndex = index;
  waitingForDispense = true;
}

// ------------------------------------------------
// PROCESS ESP32 MESSAGES
// ------------------------------------------------
void processEspMessage(String msg) {
  if (msg.startsWith("DISP:")) {
    // Format: DISP:SHEETS:COST
    // Example: DISP:3:1   -> 3 sheets, cost 1 peso
    int first = msg.indexOf(':');
    int second = msg.indexOf(':', first + 1);

    if (first > 0 && second > first) {
      int sheets = msg.substring(first + 1, second).toInt();
      int cost   = (int)msg.substring(second + 1).toFloat(); // pesos

      if (pendingIndex >= 0) {
        performDispense(pendingIndex, sheets, cost);
      }
    }
  } else if (msg.startsWith("ERR:")) {
    // Error from ESP32 (ex: InsufficientStock)
    String err = msg.substring(4);
    lcd.setCursor(0, 0);
    lcd.print("Machine Error  ");
    lcd.setCursor(0, 1);
    lcd.print(err.substring(0, 16));
    delay(2000);
    updateLCD();
    waitingForDispense = false;
    pendingIndex = -1;
  }
}

// ------------------------------------------------
// DISPENSE & MECHANISM
// ------------------------------------------------
void performDispense(int index, int sheets, int cost) {
  if (credits < cost) {
    lcd.setCursor(0, 0);
    lcd.print("Need P");
    lcd.print(cost);
    lcd.print(" more   ");
    delay(1500);
    updateLCD();
    waitingForDispense = false;
    pendingIndex = -1;
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print("Dispensing...  ");
  lcd.setCursor(0, 1);
  lcd.print(SIZE_CODES[index]);
  lcd.print(" x");
  lcd.print(sheets);
  lcd.print("      ");

  // Deduct credits (pesos)
  credits -= cost;
  if (credits < 0) credits = 0;

  // Run separation cycle (simulate cutting single sheets from pad)
  Servo* s = SIZE_SERVOS[index];
  runSeparationCycle(s, sheets);

  // Notify ESP32 about success
  int brandId = SIZE_BRAND_ID[index];
  const char* sizeCode = SIZE_CODES[index];

  espSerial.print("DONE:");
  espSerial.print(brandId);
  espSerial.print(":");
  espSerial.print(sizeCode);
  espSerial.print(":");
  espSerial.print(cost);
  espSerial.print(":");
  espSerial.println(sheets);

  // UI feedback
  lcd.setCursor(0, 0);
  lcd.print("Take your paper");
  delay(1500);
  updateLCD();

  waitingForDispense = false;
  pendingIndex = -1;
}

// Simulated separation / cutting cycle
// For each sheet, servo moves forward to "peel" one sheet from the glued pad
// then retracts back to neutral.
void runSeparationCycle(Servo* s, int count) {
  const int FEED_ANGLE = 120;   // forward push angle
  const int NEUTRAL_ANGLE = 10; // resting position

  for (int i = 0; i < count; i++) {
    // Push to separate one sheet
    s->write(FEED_ANGLE);
    delay(500);   // tune based on real hardware

    // Small vibration wiggle to help break glue
    s->write(NEUTRAL_ANGLE + 15);
    delay(150);
    s->write(FEED_ANGLE - 15);
    delay(150);

    // Return to neutral
    s->write(NEUTRAL_ANGLE);
    delay(500);
  }
}

void resetServos() {
  servoQ.write(10);
  servoCross.write(10);
  servoLength.write(10);
  servoWhole.write(10);
}

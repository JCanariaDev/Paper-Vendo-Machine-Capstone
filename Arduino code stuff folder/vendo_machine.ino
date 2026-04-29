#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <HX711.h>
#include <Stepper.h>

/*
  vendo_machine.ino - Final Production Version
  Master Controller for Paper & Pen Vendo
  UPDATED: Reverted back to 4x4 Membrane Keypad
*/

// --- PINS ---
const int COIN_PIN = 2;
const int LOADCELL_DOUT = 5;
const int LOADCELL_SCK = 6;
const int PEN_IR_PIN = 7;
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;
// Stepper Pins: 3, 4, 11, 12

// --- TEST BYPASS BUTTONS ---
const int BTN_TEST_PAPER = 30; // Request Budget 1/4 Paper
const int BTN_TEST_PEN = 38;   // Request Budget Pen

// --- KEYPAD CONFIG ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {22, 23, 24, 25};
byte colPins[COLS] = {27, 26, 28, 29}; // Swapped 26 and 27 to match your successful test
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- STEPPER CONFIG ---
const int stepsPerRevolution = 2048;
Stepper myStepper(stepsPerRevolution, 3, 11, 4, 12);

// --- PERIPHERALS ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo servoChange, servoPen;
HX711 scale;

// --- STATE ---
volatile float credits = 0;
bool isProcessing = false;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // To ESP32
  Serial.println("--- SYSTEM STARTING ---");
  
  myStepper.setSpeed(15);
  
  pinMode(BTN_TEST_PAPER, INPUT_PULLUP);
  pinMode(BTN_TEST_PEN, INPUT_PULLUP);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Smart Vendo V3");
  
  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);
  pinMode(PEN_IR_PIN, INPUT);
  
  servoChange.attach(SERVO_CHANGE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  servoChange.write(0);
  servoPen.write(0);
  
  scale.begin(LOADCELL_DOUT, LOADCELL_SCK);
  scale.set_scale(730.0); // Calibrated factor from your test
  scale.tare();
  
  Serial.println("Scale initialized. Machine Ready!");
  updateLCD();
}

void coinInterrupt() {
  static unsigned long lastPulse = 0;
  if (millis() - lastPulse > 50) {
    credits += 1.0; 
    lastPulse = millis();
  }
}

void loop() {
  // --- Check if coins were inserted ---
  static float lastCredits = -1;
  if (credits != lastCredits) {
    lastCredits = credits;
    updateLCD();
    Serial.println("Credits inserted! Total: P" + String((int)credits));
  }

  // --- Check Test Push Buttons (Bypass Coin Logic) ---
  if (!isProcessing) {
    if (digitalRead(BTN_TEST_PAPER) == LOW) {
      Serial.println("Test Button Pressed: Paper!");
      credits += 10.0; // Give fake credits to bypass
      handleRequest("paper", "1"); // Request Budget 1/4 Paper
      delay(500); // Debounce
    }
    else if (digitalRead(BTN_TEST_PEN) == LOW) {
      Serial.println("Test Button Pressed: Pen!");
      credits += 10.0; // Give fake credits to bypass
      handleRequest("pen", "1"); // Request Budget Pen
      delay(500); // Debounce
    }
  }

  char key = keypad.getKey();
  
  if (key && !isProcessing) {
    if (key >= '1' && key <= '8') {
      handleRequest("paper", String(key));
    }
    else if (key == 'A') handleRequest("pen", "1"); // Budget Pen
    else if (key == 'B') handleRequest("pen", "2"); // Standard Pen
    else if (key == '0') returnChange();
  }
  
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    if (msg.startsWith("DISPENSE:")) performDispense(msg);
    else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  }
}

void handleRequest(String type, String id) {
  if (credits < 1) { showError("Insert Coin"); return; }
  lcd.setCursor(0,1); lcd.print("Checking Cloud..");
  isProcessing = true;
  Serial.println("Sending REQ to Cloud: " + type + " ID: " + id + " Credits: " + String(credits));
  Serial1.println("REQ:" + type + ":" + id + ":" + String(credits));
}

void performDispense(String msg) {
  int f1 = msg.indexOf(':');
  int f2 = msg.indexOf(':', f1 + 1);
  int f3 = msg.indexOf(':', f2 + 1);
  
  int totalSheets = msg.substring(f1 + 1, f2).toInt();
  float cost = msg.substring(f2 + 1, f3).toFloat();
  String name = msg.substring(f3 + 1);
  
  lcd.setCursor(0,1); lcd.print("Dispensing...   ");
  Serial.println("Received from Cloud: DISPENSE " + String(totalSheets) + " of " + name);
  
  if (totalSheets > 1) { // PAPER
    // Paper logic is paused since Stepper is now used for the Pen Dispenser
    Serial.println("Paper requested, logging DONE to cloud...");
    Serial1.println("DONE:paper:1:" + name + ":" + String(cost) + ":" + String(totalSheets));
  } else { // PEN
    // Stepper Motor Pen Dispenser (Revolver Drum Mechanism)
    // A full circle is 2048 steps. If you have 12 slots: 2048 / 12 = 171 steps
    Serial.println("Pen requested. Spinning Stepper motor 171 steps...");
    myStepper.step(171); 
    stopStepper();
    Serial.println("Pen dispensed. Logging DONE to cloud...");
    Serial1.println("DONE:pen:1:" + name + ":" + String(cost) + ":1");
  }
  
  credits -= cost;
  lcd.setCursor(0,1); lcd.print("Success! Take it");
  delay(3000);
  isProcessing = false;
  updateLCD();
}

void stopStepper() {
  digitalWrite(3, LOW); digitalWrite(4, LOW); digitalWrite(11, LOW); digitalWrite(12, LOW);
}

void returnChange() {
  if (credits <= 0) return;
  lcd.setCursor(0,1); lcd.print("Returning P" + String((int)credits));
  servoChange.write(90); delay(2000); servoChange.write(0);
  credits = 0;
  updateLCD();
}

void updateLCD() {
  lcd.setCursor(0,0);
  lcd.print("Credits: P"); lcd.print((int)credits); lcd.print("    ");
  lcd.setCursor(0,1);
  lcd.print("Ready to Serve  ");
}

void showError(String m) {
  lcd.setCursor(0,1); lcd.print(m + "           ");
  delay(2000);
  isProcessing = false;
  updateLCD();
}

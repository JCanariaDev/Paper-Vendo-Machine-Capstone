#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <HX711.h>

/*
  vendo_machine.ino - Final Production Version
  Master Controller for Paper & Pen Vendo
*/

// --- PINS ---
const int COIN_PIN = 2;
const int STEPPER_STEP = 3;
const int STEPPER_DIR = 4;
const int LOADCELL_DOUT = 5;
const int LOADCELL_SCK = 6;
const int PEN_IR_PIN = 7;
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;

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
byte colPins[COLS] = {26, 27, 28, 29};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

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
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Smart Vendo V3");
  
  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);
  
  pinMode(STEPPER_STEP, OUTPUT);
  pinMode(STEPPER_DIR, OUTPUT);
  pinMode(PEN_IR_PIN, INPUT);
  
  servoChange.attach(SERVO_CHANGE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  servoChange.write(0); // Closed
  servoPen.write(0);    // Ready
  
  scale.begin(LOADCELL_DOUT, LOADCELL_SCK);
  scale.set_scale(420.0); // Calibrate this later
  scale.tare();
  
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
  char key = keypad.getKey();
  
  if (key && !isProcessing) {
    if (key >= '1' && key <= '4') handlePaperRequest(key);
    else if (key == '5') handlePenRequest();
    else if (key == '0') returnChange();
  }
  
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    if (msg.startsWith("DISPENSE:")) performDispense(msg);
    else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  }
}

void handlePaperRequest(char key) {
  if (credits < 1) { showError("Insert Coin"); return; }
  lcd.setCursor(0,1); lcd.print("Checking Cloud..");
  isProcessing = true;
  String id = String(key);
  Serial1.println("REQ:paper:" + id + ":" + String(credits));
}

void handlePenRequest() {
  if (credits < 6) { showError("Need P6 for Pen"); return; }
  lcd.setCursor(0,1); lcd.print("Checking Cloud..");
  isProcessing = true;
  Serial1.println("REQ:pen:1:" + String(credits));
}

void performDispense(String msg) {
  // Format: DISPENSE:QTY:COST:NAME
  int f1 = msg.indexOf(':');
  int f2 = msg.indexOf(':', f1 + 1);
  int f3 = msg.indexOf(':', f2 + 1);
  
  int qty = msg.substring(f1 + 1, f2).toInt();
  float cost = msg.substring(f2 + 1, f3).toFloat();
  String name = msg.substring(f3 + 1);
  String type = (qty > 1) ? "paper" : "pen";
  String id = (qty > 1) ? "1" : "1"; // Simplified for test
  
  lcd.setCursor(0,1); lcd.print("Dispensing...   ");
  
  if (qty > 1) { // PAPER
    for(int i=0; i<qty; i++) {
       digitalWrite(STEPPER_DIR, HIGH);
       for(int x=0; x<500; x++) { // 1 sheet length
         digitalWrite(STEPPER_STEP, HIGH); delayMicroseconds(1000);
         digitalWrite(STEPPER_STEP, LOW); delayMicroseconds(1000);
       }
       delay(500);
    }
  } else { // PEN
    servoPen.write(90); delay(1000); servoPen.write(0);
  }
  
  credits -= cost;
  Serial1.println("DONE:" + type + ":" + id + ":" + name + ":" + String(cost) + ":" + String(qty));
  
  lcd.setCursor(0,1); lcd.print("Success! Take it");
  delay(3000);
  isProcessing = false;
  updateLCD();
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

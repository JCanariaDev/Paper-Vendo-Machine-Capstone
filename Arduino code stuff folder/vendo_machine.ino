#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <HX711.h>
#include <Stepper.h>

/*
  vendo_machine.ino - Final Production Version
  Master Controller for Paper & Pen Vendo
  UPDATED: Replaced 4x4 Keypad with 10 Individual Push Buttons
*/

// --- PINS ---
const int COIN_PIN = 2;
const int LOADCELL_DOUT = 5;
const int LOADCELL_SCK = 6;
const int PEN_IR_PIN = 7;
const int SERVO_CHANGE_PIN = 9;
const int SERVO_PEN_PIN = 10;
// Stepper Pins: 3, 4, 11, 12

// --- 10 BUTTON PINS ---
const int BTN_P_B_14 = 30; // Budget 1/4
const int BTN_P_B_12 = 31; // Budget 1/2
const int BTN_P_B_L  = 32; // Budget Long
const int BTN_P_B_S  = 33; // Budget Short
const int BTN_P_S_14 = 34; // Std 1/4
const int BTN_P_S_12 = 35; // Std 1/2
const int BTN_P_S_L  = 36; // Std Long
const int BTN_P_S_S  = 37; // Std Short
const int BTN_B_B    = 38; // Budget Pen
const int BTN_B_S    = 39; // Std Pen

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
  
  myStepper.setSpeed(15);
  
  // Setup 10 buttons with Pullups
  for(int i=30; i<=39; i++) pinMode(i, INPUT_PULLUP);
  
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
  scale.set_scale(420.0); 
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
  if (!isProcessing) {
    checkButtons();
  }
  
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    if (msg.startsWith("DISPENSE:")) performDispense(msg);
    else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  }
}

void checkButtons() {
  // Budget Paper (IDs 1-4)
  if(digitalRead(BTN_P_B_14) == LOW) handleRequest("paper", "1");
  else if(digitalRead(BTN_P_B_12) == LOW) handleRequest("paper", "2");
  else if(digitalRead(BTN_P_B_L)  == LOW) handleRequest("paper", "3");
  else if(digitalRead(BTN_P_B_S)  == LOW) handleRequest("paper", "4");
  
  // Std Paper (IDs 5-8)
  else if(digitalRead(BTN_P_S_14) == LOW) handleRequest("paper", "5");
  else if(digitalRead(BTN_P_S_12) == LOW) handleRequest("paper", "6");
  else if(digitalRead(BTN_P_S_L)  == LOW) handleRequest("paper", "7");
  else if(digitalRead(BTN_P_S_S)  == LOW) handleRequest("paper", "8");
  
  // Ballpens (IDs 1-2)
  else if(digitalRead(BTN_B_B) == LOW) handleRequest("pen", "1");
  else if(digitalRead(BTN_B_S) == LOW) handleRequest("pen", "2");
}

void handleRequest(String type, String id) {
  if (credits < 1) { showError("Insert Coin"); return; }
  lcd.setCursor(0,1); lcd.print("Checking Cloud..");
  isProcessing = true;
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
  
  if (totalSheets > 1) { // PAPER
    myStepper.step(totalSheets * 2048);
    stopStepper(); 
    Serial1.println("DONE:paper:1:" + name + ":" + String(cost) + ":" + String(totalSheets));
  } else { // PEN
    servoPen.write(90); delay(1000); servoPen.write(0);
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

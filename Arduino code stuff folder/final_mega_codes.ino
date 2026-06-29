#include <Keypad.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <Stepper.h>

/*
  final_mega_codes.ino - Complete Production Version
  Master Controller for Paper & Pen Vendo (OLED & Custom Units Version)
  REMOVED: Load Cell Scale (HX711)
  ADDED: SSD1306 OLED, 8 Paper Steppers, 1 Paper IR Sensor, 1 Coin Hopper, Admin Mode
*/

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- PINS ---
const int COIN_PIN = 2;          // Interrupt 0 (Coin Acceptor)
const int HOPPER_SENS_PIN = 3;   // Interrupt 1 (Coin Hopper Pulse)
const int PEN_IR_PIN = 7;        // Active LOW (Pen drop detection)
const int PAPER_IR_PIN = 8;      // Active LOW (Common Paper drop detection)
const int HOPPER_CTRL_PIN = 9;   // Relay control for Hopper motor
const int SERVO_PEN_PIN = 10;    // Servo Pen (reserved/unused)

// --- TEST BYPASS BUTTONS (Moved to Analog pins to free Stepper pins) ---
const int BTN_TEST_PAPER = A0;   // Request Budget 1/4 Paper
const int BTN_TEST_PEN = A1;     // Request Budget Pen

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
byte colPins[COLS] = {27, 26, 28, 29};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- STEPPER CONFIG ---
const int stepsPerRevolution = 2048;

// Pen Stepper (moved from 3, 11, 4, 12 to 10, 11, 12, 13 to free Pin 3 for Hopper Sensor)
// Library Pin Order: IN1, IN3, IN2, IN4 -> 10, 12, 11, 13
Stepper penStepper(stepsPerRevolution, 10, 12, 11, 13);

// 8 Paper Stepper Motors
Stepper paperSteppers[8] = {
  Stepper(stepsPerRevolution, 30, 32, 31, 33), // Slot 1
  Stepper(stepsPerRevolution, 34, 36, 35, 37), // Slot 2
  Stepper(stepsPerRevolution, 38, 40, 39, 41), // Slot 3
  Stepper(stepsPerRevolution, 42, 44, 43, 45), // Slot 4
  Stepper(stepsPerRevolution, 46, 48, 47, 49), // Slot 5
  Stepper(stepsPerRevolution, 50, 52, 51, 53), // Slot 6
  Stepper(stepsPerRevolution, A8, A10, A9, A11), // Slot 7
  Stepper(stepsPerRevolution, A12, A14, A13, A15) // Slot 8
};

// --- SYSTEM STATE MACHINE ---
enum State {
  STATE_IDLE,
  STATE_FETCHING,
  STATE_SELECT_QTY,
  STATE_DISPENSING,
  STATE_RETURNING_CHANGE,
  STATE_ADMIN_AUTH,
  STATE_ADMIN_MENU,
  STATE_ADMIN_UPDATE
};
State currentState = STATE_IDLE;

// --- STATE VARIABLES ---
volatile float credits = 0;
volatile int coinsDispensed = 0;
volatile unsigned long lastHopperPulse = 0;

bool isProcessing = false;
String selectedType = "";
String selectedId = "";
float selectedCost = 0.0;
int selectedSheets = 1;
int selectedStock = 0;
String selectedName = "";

String qtyInput = "";
int limitQty = 1;

// Admin variables
String adminPinInput = "";
String adminStockInput = "";
String adminSelectedType = "";
String adminSelectedId = "";

// Dynamic screen scrolling in Idle
unsigned long lastScreenScroll = 0;
int idleScreenPage = 0;

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // Communication to ESP32 Gateway
  Serial.println("--- MASTER CONTROLLER STARTING ---");
  
  // Set speeds
  penStepper.setSpeed(10);
  for (int i = 0; i < 8; i++) {
    paperSteppers[i].setSpeed(10);
  }
  
  pinMode(BTN_TEST_PAPER, INPUT_PULLUP);
  pinMode(BTN_TEST_PEN, INPUT_PULLUP);
  pinMode(PEN_IR_PIN, INPUT_PULLUP);
  pinMode(PAPER_IR_PIN, INPUT_PULLUP);
  pinMode(HOPPER_SENS_PIN, INPUT_PULLUP);
  pinMode(HOPPER_CTRL_PIN, OUTPUT);
  digitalWrite(HOPPER_CTRL_PIN, LOW); // Keep hopper motor off
  
  // Initialize OLED Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED SSD1306 allocation failed");
    for(;;); // Halt
  }
  display.clearDisplay();
  display.display();
  
  // Interrupts
  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), coinInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(HOPPER_SENS_PIN), hopperInterrupt, FALLING);
  
  Serial.println("System Initialized! Ready.");
  updateOLED();
}

void coinInterrupt() {
  static unsigned long lastPulse = 0;
  if (millis() - lastPulse > 50) {
    credits += 1.0; 
    lastPulse = millis();
  }
}

void hopperInterrupt() {
  coinsDispensed++;
  lastHopperPulse = millis();
}

void loop() {
  // --- Check Coin Acceptor Pulse Updates ---
  static float lastCredits = -1;
  if (credits != lastCredits) {
    lastCredits = credits;
    updateOLED();
    Serial.println("Credits Inserted! Total: P" + String((int)credits));
  }

  // --- Dynamic Idle Screen Rotation ---
  if (currentState == STATE_IDLE && millis() - lastScreenScroll > 3000) {
    idleScreenPage = (idleScreenPage + 1) % 3;
    lastScreenScroll = millis();
    updateOLED();
  }

  // --- Check Test Push Buttons (Bypass Coin Logic) ---
  if (currentState == STATE_IDLE) {
    if (digitalRead(BTN_TEST_PAPER) == LOW) {
      Serial.println("Test Button: Paper selected!");
      credits += 10.0;
      triggerItemFetch("paper", "1");
      delay(500); 
    }
    else if (digitalRead(BTN_TEST_PEN) == LOW) {
      Serial.println("Test Button: Pen selected!");
      credits += 10.0;
      triggerItemFetch("pen", "1");
      delay(500);
    }
  }

  // --- Parse Incoming Serial Communication from ESP32 ---
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    parseESP32Response(msg);
  }

  // --- Handle Keypad Input based on current State ---
  char key = keypad.getKey();
  if (key) {
    handleKeypress(key);
  }

  // --- Handle Active Dispensing/Refunding Background Guard ---
  if (currentState == STATE_RETURNING_CHANGE) {
    checkHopperTimeout();
  }
}

void triggerItemFetch(String type, String id) {
  if (credits < 1) { 
    showTempMessage("Insert Coin First"); 
    return; 
  }
  selectedType = type;
  selectedId = id;
  currentState = STATE_FETCHING;
  updateOLED();
  
  Serial.println("Sending GET_INFO for " + type + " ID: " + id);
  Serial1.println("GET_INFO:" + type + ":" + id);
  
  // Timeout Guard
  unsigned long startWait = millis();
  while (currentState == STATE_FETCHING && millis() - startWait < 5000) {
    if (Serial1.available()) {
      String msg = Serial1.readStringUntil('\n');
      msg.trim();
      parseESP32Response(msg);
    }
  }
  
  if (currentState == STATE_FETCHING) {
    showTempMessage("Cloud Offline");
    currentState = STATE_IDLE;
    updateOLED();
  }
}

void parseESP32Response(String msg) {
  Serial.println("Received: " + msg);
  
  if (msg.startsWith("INFO:")) {
    // Format: INFO:cost:sheetsPerUnit:stock:name
    int f1 = msg.indexOf(':');
    int f2 = msg.indexOf(':', f1 + 1);
    int f3 = msg.indexOf(':', f2 + 1);
    int f4 = msg.indexOf(':', f3 + 1);
    
    selectedCost = msg.substring(f1 + 1, f2).toFloat();
    selectedSheets = msg.substring(f2 + 1, f3).toInt();
    selectedStock = msg.substring(f3 + 1, f4).toInt();
    selectedName = msg.substring(f4 + 1);
    
    int maxAffordable = (int)(credits / selectedCost);
    int maxStockUnits = (selectedType == "paper") ? (selectedStock / selectedSheets) : selectedStock;
    
    limitQty = min(maxAffordable, maxStockUnits);
    
    if (maxAffordable == 0) {
      showTempMessage("Insuff. Credit\nNeeds P" + String((int)selectedCost));
      currentState = STATE_IDLE;
    } else if (maxStockUnits == 0) {
      showTempMessage("Out of Stock!");
      currentState = STATE_IDLE;
    } else {
      qtyInput = "";
      currentState = STATE_SELECT_QTY;
    }
    updateOLED();
  }
  else if (msg.startsWith("INFO_ERR:")) {
    showTempMessage(msg.substring(9));
    currentState = STATE_IDLE;
    updateOLED();
  }
  else if (msg.startsWith("SET_STOCK:OK")) {
    showTempMessage("Cloud Sync OK!");
    currentState = STATE_ADMIN_MENU;
    updateOLED();
  }
  else if (msg.startsWith("SET_STOCK:ERR")) {
    showTempMessage("Update Failed");
    currentState = STATE_ADMIN_MENU;
    updateOLED();
  }
}

void handleKeypress(char key) {
  switch (currentState) {
    case STATE_IDLE:
      if (key >= '1' && key <= '8') {
        triggerItemFetch("paper", String(key));
      }
      else if (key == 'A') triggerItemFetch("pen", "1"); // Budget Pen
      else if (key == 'B') triggerItemFetch("pen", "2"); // Standard Pen
      else if (key == 'D') {
        if (credits > 0) {
          startHopperRefund();
        }
      }
      else if (key == 'C') {
        adminPinInput = "";
        currentState = STATE_ADMIN_AUTH;
        updateOLED();
      }
      // Nudging features
      else if (key == '*') {
        Serial.println("Nudge Forward");
        penStepper.step(20);
        stopAllSteppers();
      }
      else if (key == '#') {
        Serial.println("Nudge Backward");
        penStepper.step(-20);
        stopAllSteppers();
      }
      break;

    case STATE_SELECT_QTY:
      if (key >= '0' && key <= '9') {
        qtyInput += key;
        int val = qtyInput.toInt();
        if (val > limitQty) {
          qtyInput = String(limitQty); // Cap at maximum limits
        }
        updateOLED();
      }
      else if (key == '*') {
        currentState = STATE_IDLE;
        updateOLED();
      }
      else if (key == '#') {
        int finalQty = qtyInput.toInt();
        if (finalQty >= 1 && finalQty <= limitQty) {
          executeDispense(finalQty);
        }
      }
      break;

    case STATE_ADMIN_AUTH:
      if (key >= '0' && key <= '9') {
        if (adminPinInput.length() < 4) {
          adminPinInput += key;
        }
        updateOLED();
      }
      else if (key == '*') {
        currentState = STATE_IDLE;
        updateOLED();
      }
      else if (key == '#') {
        if (adminPinInput == "1234") {
          currentState = STATE_ADMIN_MENU;
        } else {
          showTempMessage("Wrong PIN");
          currentState = STATE_IDLE;
        }
        updateOLED();
      }
      break;

    case STATE_ADMIN_MENU:
      if (key >= '1' && key <= '8') {
        adminSelectedType = "paper";
        adminSelectedId = String(key);
        adminStockInput = "";
        currentState = STATE_ADMIN_UPDATE;
      }
      else if (key == 'A') {
        adminSelectedType = "pen";
        adminSelectedId = "1";
        adminStockInput = "";
        currentState = STATE_ADMIN_UPDATE;
      }
      else if (key == 'B') {
        adminSelectedType = "pen";
        adminSelectedId = "2";
        adminStockInput = "";
        currentState = STATE_ADMIN_UPDATE;
      }
      else if (key == '*') {
        currentState = STATE_IDLE;
      }
      updateOLED();
      break;

    case STATE_ADMIN_UPDATE:
      if (key >= '0' && key <= '9') {
        adminStockInput += key;
        updateOLED();
      }
      else if (key == '*') {
        currentState = STATE_ADMIN_MENU;
        updateOLED();
      }
      else if (key == '#') {
        if (adminStockInput.length() > 0) {
          currentState = STATE_FETCHING; // Use fetching screen for loader
          updateOLED();
          Serial.println("Updating Cloud Stock: " + adminSelectedType + " ID: " + adminSelectedId + " to: " + adminStockInput);
          Serial1.println("SET_STOCK:" + adminSelectedType + ":" + adminSelectedId + ":" + adminStockInput);
        }
      }
      break;

    default:
      break;
  }
}

void executeDispense(int qty) {
  currentState = STATE_DISPENSING;
  updateOLED();
  
  float totalCost = qty * selectedCost;
  bool dispenseSuccess = true;
  
  if (selectedType == "paper") {
    int totalSheets = qty * selectedSheets;
    int motorIdx = selectedId.toInt() - 1;
    
    Serial.println("Dispensing Paper Slot: " + String(motorIdx) + " Sheets: " + String(totalSheets));
    
    for (int sheet = 1; sheet <= totalSheets; sheet++) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0);
      display.print("Dispensing Paper...");
      display.setCursor(0,20);
      display.print("Sheet " + String(sheet) + " of " + String(totalSheets));
      display.display();
      
      // Start rotating slot stepper
      unsigned long stepStart = millis();
      bool sheetDropped = false;
      
      while (millis() - stepStart < 6000) { // 6 second timeout per sheet
        paperSteppers[motorIdx].step(10); // Rotate small increments
        
        if (digitalRead(PAPER_IR_PIN) == LOW) { // IR beam broken
          sheetDropped = true;
          Serial.println("Sheet " + String(sheet) + " Drop Detected!");
          // Nudge a bit more to completely discharge the paper sheet
          paperSteppers[motorIdx].step(200); 
          break;
        }
        delay(1);
      }
      
      stopAllSteppers();
      
      if (!sheetDropped) {
        dispenseSuccess = false;
        Serial.println("ERROR: Paper Jam or Empty Slot!");
        break;
      }
      delay(800); // Wait for physical stability before next sheet
    }
    
    if (dispenseSuccess) {
      credits -= totalCost;
      Serial.println("Dispense Successful! Logging transaction...");
      Serial1.println("DONE:paper:" + selectedId + ":" + selectedName + ":" + String(totalCost) + ":" + String(totalSheets));
      showTempMessage("Success!\nTake your paper");
    } else {
      showTempMessage("Paper Jam!\nCall Staff");
    }
  } 
  else { // PEN DISPENSING
    Serial.println("Dispensing Pen: " + selectedName + " Qty: " + String(qty));
    
    for (int item = 1; item <= qty; item++) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0,0);
      display.print("Dispensing Pen...");
      display.setCursor(0,20);
      display.print("Pen " + String(item) + " of " + String(qty));
      display.display();
      
      // Swing stepper to DROP position (180 deg turns)
      penStepper.step(1024);
      
      // Wait for Pen drop detection
      unsigned long waitStart = millis();
      bool penDropped = false;
      
      while (millis() - waitStart < 5000) {
        if (digitalRead(PEN_IR_PIN) == LOW) {
          penDropped = true;
          Serial.println("Pen drop detected!");
          break;
        }
        delay(1);
      }
      
      delay(500);
      
      // Swing stepper back to Catch position
      penStepper.step(-1024);
      stopAllSteppers();
      
      if (!penDropped) {
        dispenseSuccess = false;
        Serial.println("ERROR: Pen Dispense failed!");
        break;
      }
      delay(800);
    }
    
    if (dispenseSuccess) {
      credits -= totalCost;
      Serial.println("Dispense Successful! Logging Pen transaction...");
      Serial1.println("DONE:pen:" + selectedId + ":" + selectedName + ":" + String(totalCost) + ":" + String(qty));
      showTempMessage("Success!\nTake your pen");
    } else {
      showTempMessage("Pen Jammed!\nCall Staff");
    }
  }
  
  currentState = STATE_IDLE;
  updateOLED();
}

void startHopperRefund() {
  currentState = STATE_RETURNING_CHANGE;
  coinsDispensed = 0;
  lastHopperPulse = millis();
  updateOLED();
  
  int targetCoins = (int)credits;
  Serial.println("Starting Coin Hopper. Dispense target: " + String(targetCoins) + " coins.");
  
  digitalWrite(HOPPER_CTRL_PIN, HIGH); // Turn hopper relay ON
}

void checkHopperTimeout() {
  int targetCoins = (int)credits;
  
  if (coinsDispensed >= targetCoins) {
    digitalWrite(HOPPER_CTRL_PIN, LOW); // Turn hopper relay OFF
    credits = 0;
    showTempMessage("Change returned\nTake coins");
    currentState = STATE_IDLE;
    updateOLED();
  }
  // If motor is running but no pulse is received for over 3 seconds, assume empty
  else if (millis() - lastHopperPulse > 3000) {
    digitalWrite(HOPPER_CTRL_PIN, LOW); // Turn hopper relay OFF
    credits -= coinsDispensed;
    showTempMessage("Hopper Empty!\nCredit: P" + String((int)credits));
    currentState = STATE_IDLE;
    updateOLED();
  }
}

void stopAllSteppers() {
  // Release torque and prevent overheating
  for (int i = 30; i <= 53; i++) digitalWrite(i, LOW);
  digitalWrite(A8, LOW); digitalWrite(A9, LOW); digitalWrite(A10, LOW); digitalWrite(A11, LOW);
  digitalWrite(A12, LOW); digitalWrite(A13, LOW); digitalWrite(A14, LOW); digitalWrite(A15, LOW);
  digitalWrite(10, LOW); digitalWrite(11, LOW); digitalWrite(12, LOW); digitalWrite(13, LOW);
}

void updateOLED() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  // Header Rail
  display.setCursor(0, 0);
  display.print("Credits: P");
  display.print((int)credits);
  display.print(".00");
  
  display.drawFastHLine(0, 9, 128, SSD1306_WHITE);
  
  switch (currentState) {
    case STATE_IDLE:
      if (idleScreenPage == 0) {
        display.setCursor(0, 12);
        display.print("[1-4] Paper Slots (P1)");
        display.setCursor(0, 24);
        display.print("1: Budget 1/4");
        display.setCursor(0, 34);
        display.print("2: Budget Crosswise");
        display.setCursor(0, 44);
        display.print("3: Budget Lengthwise");
        display.setCursor(0, 54);
        display.print("4: Budget Whole");
      }
      else if (idleScreenPage == 1) {
        display.setCursor(0, 12);
        display.print("[5-8] Paper Slots (P2)");
        display.setCursor(0, 24);
        display.print("5: Std 1/4");
        display.setCursor(0, 34);
        display.print("6: Std Crosswise");
        display.setCursor(0, 44);
        display.print("7: Std Lengthwise");
        display.setCursor(0, 54);
        display.print("8: Std Whole");
      }
      else if (idleScreenPage == 2) {
        display.setCursor(0, 12);
        display.print("[A/B] Pens | [D] Change");
        display.setCursor(0, 24);
        display.print("A: Budget Pen (P5)");
        display.setCursor(0, 34);
        display.print("B: Std Pen (P10)");
        display.setCursor(0, 48);
        display.print("[D] Return  [C] Admin");
      }
      break;
      
    case STATE_FETCHING:
      display.setCursor(0, 24);
      display.print("Checking Cloud...");
      display.setCursor(0, 38);
      display.print("Please wait...");
      break;
      
    case STATE_SELECT_QTY:
      display.setCursor(0, 12);
      display.print("Name: " + selectedName);
      display.setCursor(0, 22);
      display.print("Max Units: " + String(limitQty));
      display.setCursor(0, 34);
      display.print("Qty (1-" + String(limitQty) + "): " + qtyInput);
      display.setCursor(0, 52);
      display.print("[#]OK   [*]Cancel");
      break;
      
    case STATE_DISPENSING:
      display.setCursor(0, 24);
      display.print("Dispensing...");
      display.setCursor(0, 38);
      display.print("Do not touch!");
      break;
      
    case STATE_RETURNING_CHANGE:
      display.setCursor(0, 20);
      display.print("Returning Change...");
      display.setCursor(0, 36);
      display.print("Returned: P" + String(coinsDispensed));
      break;
      
    case STATE_ADMIN_AUTH:
      display.setCursor(0, 14);
      display.print("--- ADMIN LOGIN ---");
      display.setCursor(0, 28);
      display.print("Enter PIN: ");
      for (int i = 0; i < adminPinInput.length(); i++) display.print("*");
      display.setCursor(0, 48);
      display.print("[#]Login  [*]Back");
      break;
      
    case STATE_ADMIN_MENU:
      display.setCursor(0, 12);
      display.print("ADMIN: SELECT SLOT");
      display.setCursor(0, 24);
      display.print("[1-8] Paper Slot Stock");
      display.setCursor(0, 34);
      display.print("[A] Budget Pen Stock");
      display.setCursor(0, 44);
      display.print("[B] Standard Pen Stock");
      display.setCursor(0, 54);
      display.print("[*] Exit");
      break;
      
    case STATE_ADMIN_UPDATE:
      display.setCursor(0, 12);
      display.print("UPDATE SLOT: " + adminSelectedType + " " + adminSelectedId);
      display.setCursor(0, 26);
      display.print("Enter new stock count");
      display.setCursor(0, 38);
      display.print("Stock: " + adminStockInput);
      display.setCursor(0, 52);
      display.print("[#]Sync   [*]Back");
      break;
  }
  
  display.display();
}

void showTempMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 16);
  display.print(msg);
  display.display();
  delay(2500);
}

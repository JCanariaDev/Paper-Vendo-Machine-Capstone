/*
 * Paper Vendo Machine - Arduino Code
 * Updated with IR SENSORS for Physical Stock Detection
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <SoftwareSerial.h>

// ------------------------------------------------
// PINS & CONFIG
// ------------------------------------------------
#define BTN_SIZE_Q        2  
#define BTN_SIZE_CROSS    3  
#define BTN_SIZE_LENGTH   4  
#define BTN_SIZE_WHOLE    8  
#define BTN_PEN           A0 

// SENSORS (IR OBSTACLE SENSORS)
// Normally LOW if object detected, HIGH if empty (depending on sensor type)
// We assume: LOW = OBJECT DETECTED, HIGH = EMPTY
#define SENSOR_Q          A2
#define SENSOR_CROSS      A3
#define SENSOR_LENGTH     A4
#define SENSOR_WHOLE      A5
#define SENSOR_PEN        13

#define COIN_PIN          5

Servo servoQ, servoCross, servoLength, servoWhole, servoPen;

#define SERVO_Q_PIN       9
#define SERVO_CROSS_PIN   10
#define SERVO_LENGTH_PIN  11
#define SERVO_WHOLE_PIN   12
#define SERVO_PEN_PIN     A1

LiquidCrystal_I2C lcd(0x27, 16, 2);
SoftwareSerial espSerial(6, 7);

// ------------------------------------------------
// RUNTIME STATE
// ------------------------------------------------
volatile int credits = 0;
bool waitingForDispense = false;
int pendingIndex = -1; 

unsigned long lastSensorCheck = 0;
const int SENSOR_INTERVAL = 5000; // Check every 5s

void setup() {
  Serial.begin(9600);
  espSerial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0); lcd.print("Smart Vendo V2");
  
  pinMode(BTN_SIZE_Q, INPUT_PULLUP);
  pinMode(BTN_SIZE_CROSS, INPUT_PULLUP);
  pinMode(BTN_SIZE_LENGTH, INPUT_PULLUP);
  pinMode(BTN_SIZE_WHOLE, INPUT_PULLUP);
  pinMode(BTN_PEN, INPUT_PULLUP);
  
  // Sensors
  pinMode(SENSOR_Q, INPUT);
  pinMode(SENSOR_CROSS, INPUT);
  pinMode(SENSOR_LENGTH, INPUT);
  pinMode(SENSOR_WHOLE, INPUT);
  pinMode(SENSOR_PEN, INPUT);
  
  pinMode(COIN_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(COIN_PIN), [](){ 
    static unsigned long lastPulse = 0;
    if (millis() - lastPulse > 50) { credits++; lastPulse = millis(); }
  }, RISING);

  servoQ.attach(SERVO_Q_PIN);
  servoCross.attach(SERVO_CROSS_PIN);
  servoLength.attach(SERVO_LENGTH_PIN);
  servoWhole.attach(SERVO_WHOLE_PIN);
  servoPen.attach(SERVO_PEN_PIN);
  
  resetServos();
  updateLCD();
}

void loop() {
  if (!waitingForDispense) {
    if (digitalRead(BTN_SIZE_Q) == LOW) handlePaper(0);
    if (digitalRead(BTN_SIZE_CROSS) == LOW) handlePaper(1);
    if (digitalRead(BTN_SIZE_LENGTH) == LOW) handlePaper(2);
    if (digitalRead(BTN_SIZE_WHOLE) == LOW) handlePaper(3);
    if (digitalRead(BTN_PEN) == LOW) handlePen();
  }

  // Periodic sensor reporting to ESP32
  if (millis() - lastSensorCheck > SENSOR_INTERVAL) {
    reportSensors();
    lastSensorCheck = millis();
  }

  if (espSerial.available()) {
    String msg = espSerial.readStringUntil('\n');
    msg.trim();
    if (msg.length() > 0) processEspMessage(msg);
  }
}

void reportSensors() {
  // Protocol: SENS:Q:CROSS:LENGTH:WHOLE:PEN
  // 1 = EMPTY, 0 = GOOD
  int q = digitalRead(SENSOR_Q);
  int c = digitalRead(SENSOR_CROSS);
  int l = digitalRead(SENSOR_LENGTH);
  int w = digitalRead(SENSOR_WHOLE);
  int p = digitalRead(SENSOR_PEN);
  
  espSerial.print("SENS:");
  espSerial.print(q); espSerial.print(":");
  espSerial.print(c); espSerial.print(":");
  espSerial.print(l); espSerial.print(":");
  espSerial.print(w); espSerial.print(":");
  espSerial.println(p);
}

void handlePaper(int index) {
  int sensors[] = {SENSOR_Q, SENSOR_CROSS, SENSOR_LENGTH, SENSOR_WHOLE};
  if (digitalRead(sensors[index]) == HIGH) { // Sensor detects empty
    showMsg("Slot is Empty!");
    return;
  }
  
  if (credits < 1) { showMsg("Insert P1 Coin"); return; }
  const char* sizes[] = {"1/4", "crosswise", "lengthwise", "1_whole"};
  lcd.setCursor(0,0); lcd.print("Checking DB...  ");
  espSerial.print("REQ:1:"); 
  espSerial.println(sizes[index]);
  pendingIndex = index;
  waitingForDispense = true;
}

void handlePen() {
  if (digitalRead(SENSOR_PEN) == HIGH) {
    showMsg("Pens are Empty!");
    return;
  }
  if (credits < 10) { showMsg("Need P10 for Pen"); return; }
  lcd.setCursor(0,0); lcd.print("Checking DB...  ");
  espSerial.println("REQ:PEN");
  pendingIndex = 4;
  waitingForDispense = true;
}

void processEspMessage(String msg) {
  if (msg.startsWith("DISP:")) {
    int first = msg.indexOf(':');
    int second = msg.indexOf(':', first + 1);
    int sheets = msg.substring(first + 1, second).toInt();
    int cost = msg.substring(second + 1).toInt();
    if (credits >= cost) dispensePaper(pendingIndex, sheets, cost);
    else showMsg("Low Credit");
  } 
  else if (msg.startsWith("DISP_PEN:")) {
    int cost = msg.substring(9).toInt();
    if (credits >= cost) dispensePen(cost);
    else showMsg("Need P10 for Pen");
  }
  else if (msg.startsWith("ERR:")) {
    showMsg(msg.substring(4));
  }
  waitingForDispense = false;
}

void dispensePaper(int index, int sheets, int cost) {
  showMsg("Dispensing...   ");
  credits -= cost;
  Servo* servos[] = {&servoQ, &servoCross, &servoLength, &servoWhole};
  runCycle(servos[index], sheets);
  
  const char* sizes[] = {"1/4", "crosswise", "lengthwise", "1_whole"};
  espSerial.print("DONE:paper:1:"); 
  espSerial.print(sizes[index]); espSerial.print(":"); 
  espSerial.print(cost); espSerial.print(":"); 
  espSerial.println(sheets);
  showMsg("Success! Take it");
}

void dispensePen(int cost) {
  showMsg("Dispensing Pen  ");
  credits -= cost;
  runCycle(&servoPen, 1);
  espSerial.print("DONE:pen:1:"); 
  espSerial.print(cost); espSerial.print(":"); 
  espSerial.println(1);
  showMsg("Success! Take it");
}

void runCycle(Servo* s, int count) {
  for (int i = 0; i < count; i++) {
    s->write(120); delay(600);
    s->write(10); delay(600);
  }
}

void resetServos() {
  servoQ.write(10); servoCross.write(10); servoLength.write(10); servoWhole.write(10); servoPen.write(10);
}

void updateLCD() {
  lcd.setCursor(0, 0); lcd.print("Credits: P"); lcd.print(credits); lcd.print("    ");
  lcd.setCursor(0, 1); lcd.print("Ready to Serve  ");
}

void showMsg(String m) {
  lcd.setCursor(0, 1); lcd.print(m + "               ");
  delay(2000);
  updateLCD();
}

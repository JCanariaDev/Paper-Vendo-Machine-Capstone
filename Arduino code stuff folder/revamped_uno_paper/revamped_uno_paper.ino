/*
  ==============================================================================
  ARDUINO UNO — DEDICATED 2-BAY PAPER DISPENSER CONTROLLER (PRODUCTION)
  Controls 2 NEMA17 Stepper Motors (TMC2209 drivers) + 2 paper-exit IR sensors.
  Communicates with Arduino Mega 2560 via Hardware Serial (D0/D1) at 9600 baud.
  ==============================================================================

  PIN CONNECTIONS ON ARDUINO UNO:
  ------------------------------------------------------------------------------
  UART Communication to Mega:
    - Pin D0 (RX)  <-- Connect to Mega TX2 (Pin 16)
    - Pin D1 (TX)  --> Connect to Mega RX2 (Pin 17)
    - GND          <-- Connect to Mega GND (Common Ground)

  2x NEMA17 + TMC2209 Paper Feeder Motors:
    - Bay 1: STEP Pin D2,  DIR Pin D3,  ENABLE Pin D10 (Active LOW)
    - Bay 2: STEP Pin D4,  DIR Pin D5,  ENABLE Pin D9  (Active LOW)

  2x Paper Exit IR Sensors (INPUT_PULLUP: HIGH = beam clear, LOW = paper passing):
    - Bay 1 Exit Sensor:   Pin D11
    - Bay 2 Exit Sensor:   Pin D12

  2x Paper Level IR Sensors (INPUT_PULLUP: LOW = level above sensor):
    - Bay 1 Level Sensor:  Pin D6
    - Bay 2 Level Sensor:  Pin D7

  Power:
    - 5V & GND logic to TMC2209 drivers and IR sensors.
    - VMOT (12V) external motor supply to TMC2209 motor power rails.
  ==============================================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int MOTOR_COUNT = 2;

// Motor Pin Assignments (TMC2209 Step/Dir Mode)
const int STEP_PINS[MOTOR_COUNT]   = { 2, 4 };
const int DIR_PINS[MOTOR_COUNT]    = { 3, 5 };
const int ENABLE_PINS[MOTOR_COUNT] = { 10, 9 }; // Bay 1: D10, Bay 2: D9 (Active LOW)

// One IR sensor is installed at the paper exit of each bay.
// The sensor confirms that a sheet actually crossed the outlet.
const int PAPER_EXIT_SENSOR_PINS[MOTOR_COUNT] = { 11, 12 };
const int PAPER_EXIT_BLOCKED_LEVEL = LOW;
const int PAPER_LEVEL_SENSOR_PINS[MOTOR_COUNT] = { 6, 7 };
const int PAPER_LEVEL_HIGH_LEVEL = LOW;
// The motor feeds continuously until the exit beam is interrupted and then
// cleared. The 15-second checkpoint avoids treating a brief sensor miss as
// an empty pad; the 20-second limit is only a final jam/sensor safety stop.
const unsigned long PAPER_NO_STOCK_CONFIRM_MS = 15000;
const unsigned long PAPER_EXIT_TIMEOUT_MS = 20000;
const long MAX_STEPS_PER_SHEET = 12000;
const uint8_t PAPER_LCD_ADDRESS = 0x27;
const uint8_t PAPER_LCD_COLUMNS = 16;
const uint8_t PAPER_LCD_ROWS = 2;

const unsigned int STEP_PULSE_DELAY_US = 250;
int paperPadStock[MOTOR_COUNT] = { -1, -1 }; // -1 = not synced yet
int sheetsPerPad[MOTOR_COUNT] = { 1, 1 };
long remainingSheets[MOTOR_COUNT] = { -1, -1 };
LiquidCrystal_I2C paperLcd(PAPER_LCD_ADDRESS, PAPER_LCD_COLUMNS, PAPER_LCD_ROWS);

void showPaperLcd(const String &line1, const String &line2 = "") {
  paperLcd.clear();
  paperLcd.setCursor(0, 0);
  paperLcd.print(line1.substring(0, PAPER_LCD_COLUMNS));
  paperLcd.setCursor(0, 1);
  paperLcd.print(line2.substring(0, PAPER_LCD_COLUMNS));
}

void sendStatus();

void enableDriver(int motorIdx) {
  // Active LOW. Drives the assigned pin LOW, plus D10 if still wired to shared pin.
  if (motorIdx == 0) {
    digitalWrite(10, LOW);
  } else if (motorIdx == 1) {
    digitalWrite(9, LOW);
    digitalWrite(10, LOW); // Also drive D10 LOW in case Bay 2 is still physically on D10
  }
}

void disableDriver(int motorIdx) {
  digitalWrite(10, HIGH);
  digitalWrite(9, HIGH);
}

void disableAllDrivers() {
  digitalWrite(10, HIGH);
  digitalWrite(9, HIGH);
}

void pulseStep(int motorIdx) {
  digitalWrite(STEP_PINS[motorIdx], HIGH);
  delayMicroseconds(STEP_PULSE_DELAY_US);
  digitalWrite(STEP_PINS[motorIdx], LOW);
  delayMicroseconds(STEP_PULSE_DELAY_US);
}

bool paperBayHasStock(int bayIndex) {
  if (bayIndex < 0 || bayIndex >= MOTOR_COUNT) return false;
  return paperPadStock[bayIndex] != 0;
}

bool paperLevelIsHigh(int bayIndex) {
  if (bayIndex < 0 || bayIndex >= MOTOR_COUNT) return false;
  return digitalRead(PAPER_LEVEL_SENSOR_PINS[bayIndex]) == PAPER_LEVEL_HIGH_LEVEL;
}

bool feedOneSheet(int bayIndex) {
  if (bayIndex < 0 || bayIndex >= MOTOR_COUNT) return false;

  const int sensorPin = PAPER_EXIT_SENSOR_PINS[bayIndex];
  
  // Auto-detect baseline clear level at start of this feed
  const int clearLevel = digitalRead(sensorPin);
  
  bool paperSeen = false;
  const long MAX_STEPS = 45000;
  
  for (long step = 0; step < MAX_STEPS; step++) {
    if (Serial.available()) {
      String cmd = Serial.readStringUntil('\n');
      cmd.trim();
      if (cmd == "STOP") {
        disableAllDrivers();
        return false;
      }
    }
    pulseStep(bayIndex);

    // Sensor detects paper when state changes from the clear baseline
    bool isBlocked = (digitalRead(sensorPin) != clearLevel);
    
    if (isBlocked) {
      // Paper entered the exit sensor beam! Motor keeps running!
      paperSeen = true;
    } else if (paperSeen && !isBlocked) {
      // Paper was seen in the sensor and has now completely left the beam!
      // Stop immediately as requested:
      return true; // Sheet successfully dispensed!
    }
  }

  // If 45,000 steps reached without complete exit:
  return false;
}

// Sends software stock state for all configured bays: STATUS:HIGH,HIGH.
// Sends the physical level warning separately so LOW level is not mistaken for empty.
void sendStatus() {
  String statusMsg = "STATUS:";
  for (int i = 0; i < MOTOR_COUNT; i++) {
    if (i > 0) statusMsg += ",";
    statusMsg += paperBayHasStock(i) ? "HIGH" : "LOW";
  }
  Serial.println(statusMsg);

  String levelMsg = "LEVEL:";
  for (int i = 0; i < MOTOR_COUNT; i++) {
    if (i > 0) levelMsg += ",";
    levelMsg += paperLevelIsHigh(i) ? "HIGH" : "LOW";
  }
  Serial.println(levelMsg);
}

void syncPaperStock(int bayNum, int padStock, int unitSheets) {
  const int idx = bayNum - 1;
  if (idx < 0 || idx >= MOTOR_COUNT) return;
  const bool sameStockSnapshot = remainingSheets[idx] >= 0 &&
                                 paperPadStock[idx] == max(0, padStock) &&
                                 sheetsPerPad[idx] == max(1, unitSheets);
  paperPadStock[idx] = max(0, padStock);
  sheetsPerPad[idx] = max(1, unitSheets);
  if (!sameStockSnapshot) {
    remainingSheets[idx] = (long)paperPadStock[idx] * sheetsPerPad[idx];
  }
  sendStatus();
}

// Dispenses sheet-by-sheet and confirms each sheet at the exit IR sensor.
void dispensePaper(int bayNum, int requestedSheets, const String &paperName) {
  int idx = bayNum - 1;
  if (idx < 0 || idx >= MOTOR_COUNT) {
    Serial.println("ERR:BAD_BAY");
    return;
  }

  showPaperLcd("Dispensing", paperName);

  enableDriver(idx);
  digitalWrite(DIR_PINS[idx], HIGH); // Forward feed

  int sheetsDispensed = 0;
  for (int s = 0; s < requestedSheets; s++) {
    if (!feedOneSheet(idx)) {
      disableDriver(idx);
       showPaperLcd("Paper sensor wait", paperName);
       Serial.println("EMPTY:" + String(bayNum) + ":" + String(sheetsDispensed));
      sendStatus();
      return;
    }

    sheetsDispensed++;
    Serial.println("SHEET_OK:" + String(bayNum) + ":" + String(sheetsDispensed));
    delay(500); // 0.5s stabilization gap between sheets as requested
  }

  disableDriver(idx);
  if (remainingSheets[idx] >= 0) {
    remainingSheets[idx] = max(0L, remainingSheets[idx] - sheetsDispensed);
    paperPadStock[idx] = (remainingSheets[idx] + sheetsPerPad[idx] - 1) / sheetsPerPad[idx];
  }
  Serial.println("DONE:" + String(bayNum) + ":" + String(sheetsDispensed));
  showPaperLcd("Dispense done", paperName);
  sendStatus();
}

void jogMotor(int bayNum, long steps) {
  int idx = bayNum - 1;
  if (idx < 0 || idx >= MOTOR_COUNT) return;

  enableDriver(idx);
  digitalWrite(DIR_PINS[idx], steps >= 0 ? HIGH : LOW);
  long totalSteps = labs(steps);

  for (long s = 0; s < totalSteps; s++) {
    pulseStep(idx);
  }
  disableDriver(idx);
  Serial.println("JOG_DONE:" + String(bayNum));
}

void handleCommand(String cmd) {
  cmd.trim();
  if (cmd.startsWith("DISPENSE:")) {
    // Format: DISPENSE:<bay_num>:<sheet_count>
    int first = cmd.indexOf(':');
    int second = cmd.indexOf(':', first + 1);
    if (first > 0 && second > first) {
      int bay = cmd.substring(first + 1, second).toInt();
      int third = cmd.indexOf(':', second + 1);
      int count = (third < 0) ? cmd.substring(second + 1).toInt() : cmd.substring(second + 1, third).toInt();
      String paperName = (third < 0) ? "Paper" : cmd.substring(third + 1);
      paperName.trim();
      dispensePaper(bay, count, paperName);
    }
  }
  else if (cmd.startsWith("STOCK:")) {
    // Format: STOCK:<bay_num>:<pad_stock>:<sheets_per_pad>
    int first = cmd.indexOf(':');
    int second = cmd.indexOf(':', first + 1);
    int third = cmd.indexOf(':', second + 1);
    if (first > 0 && second > first && third > second) {
      int bay = cmd.substring(first + 1, second).toInt();
      int pads = cmd.substring(second + 1, third).toInt();
      int sheets = cmd.substring(third + 1).toInt();
      syncPaperStock(bay, pads, sheets);
    }
  }
  else if (cmd == "STATUS?") {
    sendStatus();
  }
  else if (cmd.startsWith("JOG:")) {
    // Format: JOG:<bay_num>:<steps>
    int first = cmd.indexOf(':');
    int second = cmd.indexOf(':', first + 1);
    if (first > 0 && second > first) {
      int bay = cmd.substring(first + 1, second).toInt();
      long steps = cmd.substring(second + 1).toInt();
      jogMotor(bay, steps);
    }
  }
  else if (cmd == "STOP") {
    disableAllDrivers();
    Serial.println("STOP_OK");
  }
}

void setup() {
  Serial.begin(9600); // UART Serial to Mega
  Wire.begin();
  paperLcd.init();
  paperLcd.backlight();
  showPaperLcd("Paper dispenser", "Ready");

  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(ENABLE_PINS[i], OUTPUT);
    digitalWrite(ENABLE_PINS[i], HIGH); // Start with motors disabled (HIGH)
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(PAPER_EXIT_SENSOR_PINS[i], INPUT_PULLUP);
    pinMode(PAPER_LEVEL_SENSOR_PINS[i], INPUT_PULLUP);
    digitalWrite(STEP_PINS[i], LOW);
    digitalWrite(DIR_PINS[i], LOW);
  }

  delay(200);
  Serial.println("UNO_PAPER_READY");
}

void loop() {
  if (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    handleCommand(incoming);
  }
}

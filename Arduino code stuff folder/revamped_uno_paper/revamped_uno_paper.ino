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

const unsigned int STEP_PULSE_DELAY_US = 800; // 800us provides strong starting torque & prevents NEMA motor stall
int paperPadStock[MOTOR_COUNT] = { -1, -1 }; // -1 = not synced yet
int sheetsPerPad[MOTOR_COUNT] = { 1, 1 };
long remainingSheets[MOTOR_COUNT] = { -1, -1 };
LiquidCrystal_I2C paperLcd(PAPER_LCD_ADDRESS, PAPER_LCD_COLUMNS, PAPER_LCD_ROWS);
bool hasPaperLcd = false;

void showPaperLcd(const String &line1, const String &line2 = "") {
  if (!hasPaperLcd) return;
  paperLcd.clear();
  paperLcd.setCursor(0, 0);
  paperLcd.print(line1.substring(0, PAPER_LCD_COLUMNS));
  paperLcd.setCursor(0, 1);
  paperLcd.print(line2.substring(0, PAPER_LCD_COLUMNS));
}

void sendStatus();

void enableDriver(int motorIdx) {
  // Active LOW for TMC2209 EN pin (LOW = Driver enabled / holding torque ON)
  if (motorIdx >= 0 && motorIdx < MOTOR_COUNT) {
    // Strictly disable all other motors first so ONLY ONE motor ever draws power!
    for (int i = 0; i < MOTOR_COUNT; i++) {
      if (i != motorIdx) {
        digitalWrite(ENABLE_PINS[i], HIGH); // Other driver OFF
      }
    }
    digitalWrite(ENABLE_PINS[motorIdx], LOW); // ONLY the target driver ON

    Serial.print("DRIVER_ACTIVE: Bay ");
    Serial.print(motorIdx + 1);
    Serial.print(" (Pin D");
    Serial.print(ENABLE_PINS[motorIdx]);
    Serial.print(" is LOW/ON). Bay ");
    Serial.print((1 - motorIdx) + 1);
    Serial.print(" (Pin D");
    Serial.print(ENABLE_PINS[1 - motorIdx]);
    Serial.println(" is HIGH/OFF)");
  } else {
    // Test mode only: enable both
    for (int i = 0; i < MOTOR_COUNT; i++) digitalWrite(ENABLE_PINS[i], LOW);
    Serial.println("DRIVER_ACTIVE: Both Bay 1 & Bay 2 ON (Test Mode)");
  }
  digitalWrite(13, HIGH); // LED ON indicates motor active
}

void disableDriver(int motorIdx) {
  // Active LOW for TMC2209 EN pin (HIGH = Driver disabled / unenergized)
  if (motorIdx >= 0 && motorIdx < MOTOR_COUNT) {
    digitalWrite(ENABLE_PINS[motorIdx], HIGH);
  }
  digitalWrite(13, LOW); // LED OFF indicates motor idle
}

void disableAllDrivers() {
  for (int i = 0; i < MOTOR_COUNT; i++) {
    digitalWrite(ENABLE_PINS[i], HIGH);
  }
  digitalWrite(13, LOW);
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
  int blockedStreak = 0;
  int clearStreak = 0;
  const int NOISE_FILTER_STEPS = 30;
  const long MAX_STEPS = 5000; // ~8 seconds max per sheet at 800us step timing
  
  for (long step = 0; step < MAX_STEPS; step++) {
    // Non-blocking serial check for emergency STOP without freezing the stepper loop
    if (Serial.available()) {
      char c = Serial.peek();
      if (c == 'S') {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "STOP") {
          disableAllDrivers();
          return false;
        }
      } else {
        Serial.read(); // Discard noise / unhandled bytes
      }
    }
    pulseStep(bayIndex);

    // Sensor detects paper when state changes from the clear baseline
    bool isBlocked = (digitalRead(sensorPin) != clearLevel);
    
    if (isBlocked) {
      blockedStreak++;
      clearStreak = 0;
      if (blockedStreak >= NOISE_FILTER_STEPS) {
        // Paper has genuinely entered the exit sensor beam!
        paperSeen = true;
      }
    } else {
      clearStreak++;
      blockedStreak = 0;
      if (paperSeen && clearStreak >= NOISE_FILTER_STEPS) {
        // Paper was confirmed in the beam and has now completely left!
        return true; // Sheet successfully dispensed!
      }
    }
  }

  // If 5,000 steps reached (~8s) without complete exit:
  disableAllDrivers();
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
  delay(10); // Allow TMC2209 charge pump to stabilize after enable
  digitalWrite(DIR_PINS[idx], HIGH); // Forward feed

  int sheetsDispensed = 0;
  for (int s = 0; s < requestedSheets; s++) {
    if (!feedOneSheet(idx)) {
      disableAllDrivers();
      showPaperLcd("Paper sensor wait", paperName);
      Serial.println("EMPTY:" + String(bayNum) + ":" + String(sheetsDispensed));
      sendStatus();
      return;
    }

    sheetsDispensed++;
    Serial.println("SHEET_OK:" + String(bayNum) + ":" + String(sheetsDispensed));
    delay(500); // 0.5s stabilization gap between sheets as requested
  }

  disableAllDrivers();
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
  delay(10); // Allow TMC2209 charge pump to stabilize after enable
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
    Serial.println("ACK:DISPENSE"); // Immediate acknowledge back to Mega
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
  else if (cmd.startsWith("ENABLE") || cmd.startsWith("EN")) {
    // Keeps driver permanently energized so you can test holding torque / VREF
    int colon = cmd.indexOf(':');
    if (colon > 0) {
      int bay = cmd.substring(colon + 1).toInt();
      if (bay >= 1 && bay <= MOTOR_COUNT) {
        enableDriver(bay - 1);
        Serial.println("DRIVER_ENABLED: Bay " + String(bay) + " (Pin D" + String(ENABLE_PINS[bay - 1]) + " pulled LOW)");
      }
    } else {
      enableDriver(-1); // enables both
      Serial.println("DRIVERS_ENABLED: Both Bay 1 (Pin D10) & Bay 2 (Pin D9) pulled LOW");
    }
  }
  else if (cmd.startsWith("TEST:")) {
    // Format: TEST:<bay_num> (e.g. TEST:1 or TEST:2)
    int bay = cmd.substring(5).toInt();
    if (bay >= 1 && bay <= MOTOR_COUNT) {
      int idx = bay - 1;
      Serial.println("STARTING 1-SECOND ISOLATED TEST ON BAY " + String(bay) + "...");
      enableDriver(idx);
      delay(20);
      for (int i = 0; i < 1000; i++) {
        pulseStep(idx);
      }
      disableAllDrivers();
      Serial.println("TEST FINISHED: Bay " + String(bay) + " motor powered down.");
    } else {
      Serial.println("ERR: Use TEST:1 or TEST:2");
    }
  }
  else if (cmd.startsWith("DISABLE") || cmd == "DIS" || cmd == "STOP") {
    disableAllDrivers();
    Serial.println("DRIVERS_DISABLED: Bay 1 (D10) & Bay 2 (D9) set HIGH (Motors free)");
  }
}

void setup() {
  Serial.begin(9600); // UART Serial to Mega
  Serial.setTimeout(100);

  // Status LED on Uno (Pin 13)
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH); // Turn LED ON during boot

  // Configure direct TMC2209 Enable Pins: Bay 1 -> D10, Bay 2 -> D9 (Active LOW)
  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(ENABLE_PINS[i], OUTPUT);
    digitalWrite(ENABLE_PINS[i], HIGH); // HIGH = Disabled on startup
  }

  // Safe I2C probe to prevent infinite lockup if no 1602 LCD is connected
  Wire.begin();
  #if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(25000, true);
  #endif
  Wire.beginTransmission(PAPER_LCD_ADDRESS);
  if (Wire.endTransmission() == 0) {
    hasPaperLcd = true;
    paperLcd.init();
    paperLcd.backlight();
    showPaperLcd("Paper dispenser", "Ready");
  } else {
    hasPaperLcd = false;
  }

  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(PAPER_EXIT_SENSOR_PINS[i], INPUT_PULLUP);
    pinMode(PAPER_LEVEL_SENSOR_PINS[i], INPUT_PULLUP);
    digitalWrite(STEP_PINS[i], LOW);
    digitalWrite(DIR_PINS[i], LOW);
  }

  delay(200);
  digitalWrite(13, LOW); // Boot finished, LED OFF
  Serial.println("UNO_PAPER_READY");
}

void loop() {
  if (Serial.available()) {
    String incoming = Serial.readStringUntil('\n');
    handleCommand(incoming);
  }
}

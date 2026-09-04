/*
  ==============================================================================
  ARDUINO UNO — DEDICATED 2-BAY PAPER DISPENSER CONTROLLER (PRODUCTION)
  Controls 2 NEMA17 Stepper Motors (TMC2209 drivers) + 2 L5290 Tray Sensors.
  Communicates with Arduino Mega 2560 via Hardware Serial (D0/D1) at 9600 baud.
  ==============================================================================

  PIN CONNECTIONS ON ARDUINO UNO:
  ------------------------------------------------------------------------------
  UART Communication to Mega:
    - Pin D0 (RX)  <-- Connect to Mega TX2 (Pin 16)
    - Pin D1 (TX)  --> Connect to Mega RX2 (Pin 17)
    - GND          <-- Connect to Mega GND (Common Ground)

  2x NEMA17 + TMC2209 Paper Feeder Motors:
    - Bay 1: STEP Pin D2,  DIR Pin D3
    - Bay 2: STEP Pin D4,  DIR Pin D5
    - Common ENABLE Pin:   Pin D10 (Active LOW)

  2x L5290 Paper Tray Presence Sensors (INPUT_PULLUP: HIGH = Present, LOW = Empty):
    - Bay 1 Tray Sensor:   Pin D11
    - Bay 2 Tray Sensor:   Pin D12

  Power:
    - 5V & GND logic to TMC2209 drivers and L5290 sensors.
    - VMOT (12V) external motor supply to TMC2209 motor power rails.
  ==============================================================================
*/

const int MOTOR_COUNT = 2;

// Motor Pin Assignments (TMC2209 Step/Dir Mode)
const int STEP_PINS[MOTOR_COUNT] = { 2, 4 };
const int DIR_PINS[MOTOR_COUNT]  = { 3, 5 };
const int ENABLE_PIN             = 10; // Common active LOW

// L5290 Paper Tray Presence Sensors
const int TRAY_SENSOR_PINS[MOTOR_COUNT] = { 11, 12 };

const unsigned int STEP_PULSE_DELAY_US = 900;
const int STEPS_PER_SHEET = 400; // Calibrated steps for 1 sheet feed

void enableDrivers() {
  digitalWrite(ENABLE_PIN, LOW); // Active LOW
}

void disableDrivers() {
  digitalWrite(ENABLE_PIN, HIGH);
}

void pulseStep(int motorIdx) {
  digitalWrite(STEP_PINS[motorIdx], HIGH);
  delayMicroseconds(STEP_PULSE_DELAY_US);
  digitalWrite(STEP_PINS[motorIdx], LOW);
  delayMicroseconds(STEP_PULSE_DELAY_US);
}

bool checkTrayPresence(int bayIndex) {
  if (bayIndex < 0 || bayIndex >= MOTOR_COUNT) return false;
  return (digitalRead(TRAY_SENSOR_PINS[bayIndex]) == HIGH);
}

// Sends live presence state for all configured bays: STATUS:HIGH,HIGH
void sendStatus() {
  String statusMsg = "STATUS:";
  for (int i = 0; i < MOTOR_COUNT; i++) {
    if (i > 0) statusMsg += ",";
    statusMsg += checkTrayPresence(i) ? "HIGH" : "LOW";
  }
  Serial.println(statusMsg);
}

// Dispenses sheet-by-sheet with continuous L5290 presence verification
void dispensePaper(int bayNum, int requestedSheets) {
  int idx = bayNum - 1;
  if (idx < 0 || idx >= MOTOR_COUNT) {
    Serial.println("ERR:BAD_BAY");
    return;
  }

  // 1. Pre-check L5290 Sensor
  if (!checkTrayPresence(idx)) {
    Serial.println("EMPTY:" + String(bayNum));
    return;
  }

  enableDrivers();
  digitalWrite(DIR_PINS[idx], HIGH); // Forward feed

  int sheetsDispensed = 0;
  for (int s = 0; s < requestedSheets; s++) {
    // Check L5290 before each sheet
    if (!checkTrayPresence(idx)) {
      disableDrivers();
      Serial.println("EMPTY:" + String(bayNum) + ":" + String(sheetsDispensed));
      return;
    }

    // Step motor to feed 1 sheet
    for (int step = 0; step < STEPS_PER_SHEET; step++) {
      // Continuous check during rotation
      if (!checkTrayPresence(idx)) {
        disableDrivers();
        Serial.println("EMPTY:" + String(bayNum) + ":" + String(sheetsDispensed));
        return;
      }
      pulseStep(idx);
    }

    sheetsDispensed++;
    delay(200); // Inter-sheet stabilization gap
  }

  disableDrivers();
  Serial.println("DONE:" + String(bayNum) + ":" + String(sheetsDispensed));
}

void jogMotor(int bayNum, long steps) {
  int idx = bayNum - 1;
  if (idx < 0 || idx >= MOTOR_COUNT) return;

  enableDrivers();
  digitalWrite(DIR_PINS[idx], steps >= 0 ? HIGH : LOW);
  long totalSteps = labs(steps);

  for (long s = 0; s < totalSteps; s++) {
    pulseStep(idx);
  }
  disableDrivers();
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
      int count = cmd.substring(second + 1).toInt();
      dispensePaper(bay, count);
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
}

void setup() {
  Serial.begin(9600); // UART Serial to Mega

  pinMode(ENABLE_PIN, OUTPUT);
  disableDrivers(); // Start with motors disabled

  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(TRAY_SENSOR_PINS[i], INPUT_PULLUP);
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

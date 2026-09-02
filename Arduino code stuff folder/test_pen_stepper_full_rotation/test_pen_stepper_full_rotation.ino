/*
  ==============================================================================
  TEST — PEN STEPPER (ULN2003: IN1-IN4) + IR DROP SENSOR (360° FULL ROTATION)
  Arduino UNO + 28BYJ-48 Stepper + ULN2003 Driver + IR Sensor Module
  ==============================================================================

  WORKFLOW:
  ------------------------------------------------------------------------------
    1. Type "ON" in Serial Monitor.
    2. Motor rotates FORWARD 1 FULL 360° ROTATION (4096 half-steps) to dispense.
    3. Motor pauses and monitors the IR sensor on Pin D7 for 3 to 5 seconds.
       - If IR DETECTS the pen drop -> logs drop confirmed immediately.
       - If 3-5 seconds PASS with NO drop -> logs timeout.
    4. Motor rotates REVERSE 360° (4096 half-steps) back to its starting position.
    5. Motor coils are de-energized (all LOW) so the driver/motor remains cool.

  WIRING — Arduino UNO to ULN2003 Driver & IR Sensor:
  ------------------------------------------------------------------------------
    Arduino UNO         ULN2003 Driver Module
    ─────────────       ───────────────────────
    Pin D8       -->    IN1
    Pin D9       -->    IN2
    Pin D10      -->    IN3
    Pin D11      -->    IN4
    5V           -->    5V+ / VDD (or external 5V power supply)
    GND          -->    GND       (must share COMMON GND with Arduino)

    Arduino UNO         IR Sensor Module (Drop Sensor)
    ─────────────       ──────────────────────────────
    Pin D7       -->    OUT / DO (Digital Output)
    5V           -->    VCC
    GND          -->    GND

    Motor Connection:
    -----------------
    Plug the 5-pin white connector of the 28BYJ-48 stepper directly into the
    ULN2003 driver board socket.
  ==============================================================================
*/

// ── PIN ASSIGNMENTS ──────────────────────────────────────────────────────────
const int IN1_PIN = 8;
const int IN2_PIN = 9;
const int IN3_PIN = 10;
const int IN4_PIN = 11;

const int IR_SENSOR_PIN = 7;  // Digital pin for IR sensor (OUT / DO)

// ── IR SENSOR LOGIC LEVEL ─────────────────────────────────────────────────────
// Most IR obstacle/beam sensors output LOW when an object is detected.
// If your sensor outputs HIGH when triggered, change this to HIGH.
const int IR_DETECTED_STATE = LOW;

// ── STEPPER CALIBRATION (28BYJ-48 in 8-step half-stepping mode) ───────────────
// 4096 half-steps = 360° (1 FULL REVOLUTION) -> DEFAULT
// 2048 half-steps = 180° (Half rotation)
// 1024 half-steps = 90°  (Quarter rotation)
int stepsToDrop     = 4096;   // 360° full rotation (configurable via Serial)
int waitTimeoutSec  = 3;      // Seconds to wait at drop position for IR sensor
int stepDelayUs     = 1000;   // Delay between steps in microseconds (800-1200us)

// ── 8-STEP HALF-STEPPING SEQUENCE (Smoother, higher torque) ───────────────────
const int STEP_PHASES[8][4] = {
  {HIGH, LOW,  LOW,  LOW }, // Phase 0
  {HIGH, HIGH, LOW,  LOW }, // Phase 1
  {LOW,  HIGH, LOW,  LOW }, // Phase 2
  {LOW,  HIGH, HIGH, LOW }, // Phase 3
  {LOW,  LOW,  HIGH, LOW }, // Phase 4
  {LOW,  LOW,  HIGH, HIGH}, // Phase 5
  {LOW,  LOW,  LOW,  HIGH}, // Phase 6
  {HIGH, LOW,  LOW,  HIGH}  // Phase 7
};

int currentPhase = 0;

// ── LOW LEVEL STEPPER FUNCTIONS ───────────────────────────────────────────────
void applyPhase(int phase) {
  digitalWrite(IN1_PIN, STEP_PHASES[phase][0]);
  digitalWrite(IN2_PIN, STEP_PHASES[phase][1]);
  digitalWrite(IN3_PIN, STEP_PHASES[phase][2]);
  digitalWrite(IN4_PIN, STEP_PHASES[phase][3]);
}

// Power off all coils so motor/driver doesn't get hot while idling
void releaseMotor() {
  digitalWrite(IN1_PIN, LOW);
  digitalWrite(IN2_PIN, LOW);
  digitalWrite(IN3_PIN, LOW);
  digitalWrite(IN4_PIN, LOW);
}

void stepOnce(bool forward) {
  if (forward) {
    currentPhase = (currentPhase + 1) % 8;
  } else {
    currentPhase = (currentPhase - 1 + 8) % 8;
  }
  applyPhase(currentPhase);
  delayMicroseconds(stepDelayUs);
}

bool isPenDetected() {
  return (digitalRead(IR_SENSOR_PIN) == IR_DETECTED_STATE);
}

// ── MAIN DISPENSE CYCLE ───────────────────────────────────────────────────────
void runPenDispenseCycle() {
  Serial.println(F("\n=================================================="));
  Serial.println(F("[1/3] ROTATING FORWARD 360°..."));
  Serial.print(F("      Stepping forward: "));
  Serial.print(stepsToDrop);
  Serial.print(F(" steps ("));
  Serial.print((float)stepsToDrop * 360.0 / 4096.0, 1);
  Serial.println(F("° full turn)"));

  // 1. Rotate Forward 360 degrees
  for (int i = 0; i < stepsToDrop; i++) {
    stepOnce(true); // forward
  }

  // 2. Pause & Monitor IR Sensor for 3 - 5 seconds
  Serial.println(F("[2/3] HOLDING POSITION..."));
  Serial.print(F("      Waiting up to "));
  Serial.print(waitTimeoutSec);
  Serial.println(F(" seconds for IR sensor to detect pen drop..."));

  bool penDetected = false;
  unsigned long startTime = millis();
  unsigned long timeoutDuration = (unsigned long)waitTimeoutSec * 1000;

  while (millis() - startTime < timeoutDuration) {
    if (isPenDetected()) {
      penDetected = true;
      Serial.println(F("      >>> [IR DETECTED!] Pen drop confirmed! <<<"));
      break;
    }
    delay(10); // Polling check
  }

  if (!penDetected) {
    Serial.println(F("      [TIMEOUT] No pen detected within time limit."));
  }

  // Brief stabilization gap
  delay(300);

  // 3. Rotate Reverse back to exact starting/home position
  Serial.println(F("[3/3] ROTATING BACK 360° TO PREVIOUS POSITION..."));
  for (int i = 0; i < stepsToDrop; i++) {
    stepOnce(false); // reverse
  }

  // 4. De-energize coils
  releaseMotor();

  Serial.println(F("[COMPLETE] Rotor returned to starting position. Coils off."));
  if (penDetected) {
    Serial.println(F("STATUS: SUCCESS (Pen dropped & verified by IR)"));
  } else {
    Serial.println(F("STATUS: TIMEOUT / NO DROP (Rotor safely reset to start)"));
  }
  Serial.println(F("=================================================="));
  Serial.println(F("Type ON to test again, or HELP for options.\n"));
}

// ── LIVE IR SENSOR TEST ───────────────────────────────────────────────────────
void testIRSensor() {
  Serial.println(F("\n=== LIVE IR SENSOR TEST (5 Seconds) ==="));
  Serial.println(F("Wave a pen or hand across the IR sensor..."));
  
  unsigned long start = millis();
  while (millis() - start < 5000) {
    bool detected = isPenDetected();
    Serial.print(F("IR Sensor: "));
    if (detected) {
      Serial.println(F(">> [OBJECT DETECTED] <<"));
    } else {
      Serial.println(F("CLEAR (No object)"));
    }
    delay(250);
  }
  Serial.println(F("=== Test Finished. ===\n"));
}

// ── PRINT HELP / STATUS ───────────────────────────────────────────────────────
void printHelp() {
  Serial.println(F("\n--- COMMAND LIST ---"));
  Serial.println(F("  ON          : Run 360° dispense test (Rotate 360° -> Wait IR -> Reverse 360°)"));
  Serial.println(F("  IR          : Live test IR sensor for 5 seconds"));
  Serial.println(F("  STEPS <n>   : Set rotation steps (Default 4096 for 360°, 2048 for 180°)"));
  Serial.println(F("  WAIT <sec>  : Set IR wait timeout in seconds (e.g. WAIT 3 or WAIT 5)"));
  Serial.println(F("  SPEED <us>  : Set step delay (e.g. SPEED 1000, lower = faster)"));
  Serial.println(F("  STATUS      : Show current settings"));
  Serial.println(F("  HELP        : Show this command menu"));
  Serial.println(F("--------------------\n"));
}

void printStatus() {
  Serial.println(F("\n--- CURRENT SETTINGS ---"));
  Serial.print(F("  Steps to Drop   : ")); Serial.print(stepsToDrop);
  Serial.print(F(" (approx ")); Serial.print((float)stepsToDrop * 360.0 / 4096.0, 1); Serial.println(F(" degrees)"));
  Serial.print(F("  IR Wait Timeout : ")); Serial.print(waitTimeoutSec); Serial.println(F(" seconds"));
  Serial.print(F("  Step Speed Delay: ")); Serial.print(stepDelayUs); Serial.println(F(" us"));
  Serial.println(F("------------------------\n"));
}

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  pinMode(IN3_PIN, OUTPUT);
  pinMode(IN4_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);

  releaseMotor(); // Start with coils off

  Serial.println(F("=================================================="));
  Serial.println(F("  PEN DISPENSER TEST — 360° ROTATION (ULN2003)    "));
  Serial.println(F("  Arduino UNO + 28BYJ-48 Stepper + IR Sensor      "));
  Serial.println(F("=================================================="));
  printStatus();
  printHelp();
}

// ── LOOP ──────────────────────────────────────────────────────────────────────
void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "ON") {
      runPenDispenseCycle();
    }
    else if (cmd == "IR") {
      testIRSensor();
    }
    else if (cmd.startsWith("STEPS ")) {
      int val = cmd.substring(6).toInt();
      if (val > 0 && val <= 8192) {
        stepsToDrop = val;
        Serial.print(F("[CONFIG] Steps set to: "));
        Serial.print(stepsToDrop);
        Serial.print(F(" (~"));
        Serial.print((float)stepsToDrop * 360.0 / 4096.0, 1);
        Serial.println(F(" deg)"));
      } else {
        Serial.println(F("[ERR] Enter a value between 200 and 8192 (e.g. STEPS 4096)"));
      }
    }
    else if (cmd.startsWith("WAIT ")) {
      int val = cmd.substring(5).toInt();
      if (val >= 1 && val <= 10) {
        waitTimeoutSec = val;
        Serial.print(F("[CONFIG] IR timeout set to: "));
        Serial.print(waitTimeoutSec);
        Serial.println(F(" seconds"));
      } else {
        Serial.println(F("[ERR] Enter seconds between 1 and 10 (e.g. WAIT 3)"));
      }
    }
    else if (cmd.startsWith("SPEED ")) {
      int val = cmd.substring(6).toInt();
      if (val >= 600 && val <= 3000) {
        stepDelayUs = val;
        Serial.print(F("[CONFIG] Step delay set to: "));
        Serial.print(stepDelayUs);
        Serial.println(F(" us"));
      } else {
        Serial.println(F("[ERR] Enter speed delay between 600 and 3000 (e.g. SPEED 1000)"));
      }
    }
    else if (cmd == "STATUS") {
      printStatus();
    }
    else if (cmd == "HELP") {
      printHelp();
    }
    else if (cmd.length() > 0) {
      Serial.print(F("[?] Unknown command: "));
      Serial.println(cmd);
      Serial.println(F("    Type ON to start or HELP for commands."));
    }
  }
}

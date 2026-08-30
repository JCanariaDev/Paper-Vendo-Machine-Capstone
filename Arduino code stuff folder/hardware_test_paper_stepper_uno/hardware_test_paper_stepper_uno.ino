/*
  ==============================================================================
  HARDWARE TEST — SINGLE NEMA17 + TMC2209 STEPPER MOTOR
  Arduino UNO — Serial Monitor Control
  ==============================================================================

  PIN CONNECTIONS:
  ------------------------------------------------------------------------------
    STEP  = D2
    DIR   = D3
    ENABLE = D10  (Active LOW — LOW = enabled, HIGH = disabled/idle)

  SERIAL MONITOR COMMANDS (9600 baud):
  ------------------------------------------------------------------------------
    ON        — Spin motor indefinitely
    OFF       — Stop motor
    SHEET     — Run 400 steps (1 sheet feed)
    STEPS 800 — Run any custom number of steps
    DIR FWD   — Set direction forward (default)
    DIR REV   — Set direction reverse
    STATUS    — Print current state
    HELP      — Show command list
  ==============================================================================
*/

// ── PINS ─────────────────────────────────────────────────────────────────────
const int STEP_PIN   = 2;
const int DIR_PIN    = 3;
const int ENABLE_PIN = 10; // Active LOW

// ── SETTINGS ─────────────────────────────────────────────────────────────────
unsigned int stepDelayUs  = 250; // Microseconds per half-pulse (lower = FASTER, e.g. 100-300)
const int STEPS_PER_SHEET = 800; // Calibrate as needed

// ── STATE ─────────────────────────────────────────────────────────────────────
bool motorRunning = false;
bool dirForward   = true;

// ── HELPERS ──────────────────────────────────────────────────────────────────
void motorEnable()  { digitalWrite(ENABLE_PIN, LOW);  }
void motorDisable() { digitalWrite(ENABLE_PIN, HIGH); }

void setDir(bool fwd) {
  dirForward = fwd;
  digitalWrite(DIR_PIN, fwd ? HIGH : LOW);
}

void pulseStep() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(stepDelayUs);
  digitalWrite(STEP_PIN, LOW);
  delayMicroseconds(stepDelayUs);
}

void runSteps(int steps) {
  motorEnable();
  for (int i = 0; i < steps; i++) pulseStep();
  motorDisable();
}

void printStatus() {
  Serial.println(F("=== STATUS ==="));
  Serial.print(F("Direction : ")); Serial.println(dirForward ? F("FORWARD") : F("REVERSE"));
  Serial.print(F("Motor     : ")); Serial.println(motorRunning ? F("RUNNING") : F("STOPPED"));
  Serial.print(F("Step Delay: ")); Serial.print(stepDelayUs); Serial.println(F(" us (lower = faster)"));
  Serial.println(F("=============="));
}

void printHelp() {
  Serial.println(F("--- COMMANDS -------------------"));
  Serial.println(F("  ON          : Spin motor continuously"));
  Serial.println(F("  OFF         : Stop motor"));
  Serial.println(F("  SPEED <us>  : Change speed (e.g. SPEED 150 is fast, SPEED 500 is slow)"));
  Serial.println(F("  SHEET       : Run test sheet feed"));
  Serial.println(F("  STEPS <n>   : Run N steps (e.g. STEPS 1600)"));
  Serial.println(F("  DIR FWD     : Direction forward"));
  Serial.println(F("  DIR REV     : Direction reverse"));
  Serial.println(F("  STATUS      : Print state"));
  Serial.println(F("  HELP        : Show this menu"));
  Serial.println(F("--------------------------------"));
}

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(STEP_PIN,   OUTPUT);
  pinMode(DIR_PIN,    OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  setDir(true);
  motorDisable();

  Serial.println(F("====================================="));
  Serial.println(F(" STEPPER MOTOR TEST — ARDUINO UNO"));
  Serial.println(F("====================================="));
  printHelp();
}

// ── LOOP ──────────────────────────────────────────────────────────────────────
void loop() {
  if (motorRunning) {
    motorEnable();
    pulseStep();
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "ON") {
      motorRunning = true;
      motorEnable();
      Serial.println(F("[ON] Motor spinning..."));
    }
    else if (cmd == "OFF") {
      motorRunning = false;
      motorDisable();
      Serial.println(F("[OFF] Motor stopped."));
    }
    else if (cmd.startsWith("SPEED ")) {
      int spd = cmd.substring(6).toInt();
      if (spd >= 50 && spd <= 2000) {
        stepDelayUs = spd;
        Serial.print(F("[SPEED] Step delay set to: ")); Serial.print(stepDelayUs);
        Serial.println(F(" us (lower = faster)"));
      } else {
        Serial.println(F("[ERR] Enter a delay between 50 and 2000 (e.g. SPEED 150)"));
      }
    }
    else if (cmd == "SHEET") {
      motorRunning = false;
      Serial.print(F("[SHEET] Running steps..."));
      runSteps(STEPS_PER_SHEET);
      Serial.println(F(" Done."));
    }
    else if (cmd.startsWith("STEPS ")) {
      motorRunning = false;
      int steps = cmd.substring(6).toInt();
      if (steps > 0) {
        Serial.print(F("[STEPS] Running ")); Serial.print(steps); Serial.println(F(" steps..."));
        runSteps(steps);
        Serial.println(F("[STEPS] Done."));
      } else {
        Serial.println(F("[ERR] Example: STEPS 1600"));
      }
    }
    else if (cmd == "DIR FWD") {
      setDir(true);
      Serial.println(F("[DIR] Forward."));
    }
    else if (cmd == "DIR REV") {
      setDir(false);
      Serial.println(F("[DIR] Reverse."));
    }
    else if (cmd == "STATUS") {
      printStatus();
    }
    else if (cmd == "HELP") {
      printHelp();
    }
    else {
      Serial.print(F("[?] Unknown: ")); Serial.println(cmd);
    }
  }
}

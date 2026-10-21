/*
  ==============================================================================
  ARDUINO UNO - BALLPEN HARDWARE DIAGNOSTIC & TEST SKETCH
  Target Board: Arduino Uno (Ballpen Controller)
  Features:
    1. Automatic Stepper activation when IR Drop Sensor detects an object.
    2. Serial Monitor CLI (commands to test stepper, individual coils, LEDs, buzzer).
    3. Hardware ULN2003 diagnostic (walking LED test, coil checks, power verification).
    4. Real-time IR sensor monitoring with configurable active level (LOW vs HIGH).
  ==============================================================================

  WIRING REFERENCE (Ballpen Uno):
  ------------------------------------------------------------------------------
  ULN2003 Stepper Driver:
    - IN1 -> Arduino Uno Pin D3
    - IN2 -> Arduino Uno Pin D4
    - IN3 -> Arduino Uno Pin D11
    - IN4 -> Arduino Uno Pin D12
    - 5V+ -> 5V External Power Supply (or Uno 5V for light testing)
    - GND -> Arduino Uno GND (MUST share common GND!)

  Ballpen IR Sensor:
    - OUT / DO -> Arduino Uno Pin D7 (Internal PULLUP active; default LOW on detect)
    - VCC      -> 5V
    - GND      -> GND

  Indicators & Sound:
    - Green LED  -> Pin D8
    - Red LED    -> Pin D9
    - Buzzer     -> Pin D10 (Passive or Active Buzzer)
    - Blue LED   -> Pin D13 (Built-in LED / Active indicator)

  IMPORTANT UART NOTE:
    - If the Arduino Uno is connected to your PC via USB cable, disconnect wires
      on Uno Pin D0 (RX) and D1 (TX) to the Mega while testing! The PC USB-to-Serial
      chip shares Pins D0/D1 and will conflict with external UART lines.
  ==============================================================================
*/

#include <Arduino.h>

// ── PIN DEFINITIONS ──────────────────────────────────────────────────────────
int pinIN1 = 3;
int pinIN2 = 4;
int pinIN3 = 11;
int pinIN4 = 12;

const int IR_SENSOR_PIN = 7;
const int LED_GREEN_PIN = 8;
const int LED_RED_PIN   = 9;
const int BUZZER_PIN    = 10;
const int LED_BLUE_PIN  = 13;

// ── CONFIGURATION & CALIBRATION ──────────────────────────────────────────────
// Default 28BYJ-48 calibration (4-step sequence):
// 2048 steps = 360° (Full Revolution)
// 1024 steps = 180° (Half Turn)
//  512 steps =  90° (Quarter Turn)
int defaultStepsPerHalfTurn = 1024;
int stepDelayMicros = 2800; // ~10 RPM for smooth stepping without skipping
bool autoTriggerEnabled = true;
int irActiveLevel = LOW;    // Most IR sensors output LOW when beam broken / object detected
int steppingMode = 4;       // 4 = 4-step full stepping (2048 steps/rev), 8 = 8-step half stepping (4096 steps/rev)

// ── STEPPER SEQUENCES ────────────────────────────────────────────────────────
// 4-Step sequence (2-coil excitation, high torque, standard):
const int STEP_PHASES_4[4][4] = {
  {HIGH, LOW,  HIGH, LOW }, // Phase 0 (IN1 + IN3)
  {LOW,  HIGH, HIGH, LOW }, // Phase 1 (IN2 + IN3)
  {LOW,  HIGH, LOW,  HIGH}, // Phase 2 (IN2 + IN4)
  {HIGH, LOW,  LOW,  HIGH}  // Phase 3 (IN1 + IN4)
};

// 8-Step half-stepping sequence (ultra-smooth, 4096 steps/rev):
const int STEP_PHASES_8[8][4] = {
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
bool motorRunningContinuous = false;
int continuousDirection = 1;
bool stopRequested = false;

// ── STATE TRACKING ───────────────────────────────────────────────────────────
int lastIrState = -1;
unsigned long lastIrChangeTime = 0;
const unsigned long IR_DEBOUNCE_MS = 150;
bool isExecutingCycle = false;

// ── LOW LEVEL STEPPER CONTROL ────────────────────────────────────────────────
void releaseMotor() {
  digitalWrite(pinIN1, LOW);
  digitalWrite(pinIN2, LOW);
  digitalWrite(pinIN3, LOW);
  digitalWrite(pinIN4, LOW);
}

void applyPhase(int phase) {
  if (steppingMode == 8) {
    digitalWrite(pinIN1, STEP_PHASES_8[phase][0]);
    digitalWrite(pinIN2, STEP_PHASES_8[phase][1]);
    digitalWrite(pinIN3, STEP_PHASES_8[phase][2]);
    digitalWrite(pinIN4, STEP_PHASES_8[phase][3]);
  } else {
    digitalWrite(pinIN1, STEP_PHASES_4[phase][0]);
    digitalWrite(pinIN2, STEP_PHASES_4[phase][1]);
    digitalWrite(pinIN3, STEP_PHASES_4[phase][2]);
    digitalWrite(pinIN4, STEP_PHASES_4[phase][3]);
  }
}

void stepOnce(int direction) {
  int totalPhases = (steppingMode == 8) ? 8 : 4;
  if (direction >= 0) {
    currentPhase = (currentPhase + 1) % totalPhases;
  } else {
    currentPhase = (currentPhase - 1 + totalPhases) % totalPhases;
  }
  applyPhase(currentPhase);
  delayMicroseconds(stepDelayMicros);
}

bool checkSerialBreak() {
  if (Serial.available() > 0) {
    String peekCmd = Serial.readStringUntil('\n');
    peekCmd.trim();
    peekCmd.toUpperCase();
    if (peekCmd == "STOP" || peekCmd == "OFF") {
      stopRequested = true;
      motorRunningContinuous = false;
      releaseMotor();
      Serial.println(F("\n[!] EMERGENCY STOP RECEIVED: Motor halted & coils released."));
      return true;
    }
  }
  return stopRequested;
}

bool stepMotor(int steps, int direction) {
  stopRequested = false;
  for (int i = 0; i < steps; i++) {
    if (checkSerialBreak()) return false;
    stepOnce(direction);
  }
  return true;
}

// ── IR SENSOR CHECK ──────────────────────────────────────────────────────────
bool isIrDetected() {
  return digitalRead(IR_SENSOR_PIN) == irActiveLevel;
}

// ── DISPENSE MOTION CYCLE (180° forward, pause, 180° return) ────────────────
void runDispenseTestCycle(const char* triggerSource) {
  isExecutingCycle = true;
  stopRequested = false;

  Serial.println();
  Serial.print(F(">>> [CYCLE START] Triggered by: "));
  Serial.println(triggerSource);
  Serial.print(F(">>> Raw Pin D7 = "));
  Serial.print(digitalRead(IR_SENSOR_PIN) == LOW ? F("LOW (0)") : F("HIGH (1)"));
  Serial.print(F(" | Active Level = "));
  Serial.println(irActiveLevel == LOW ? F("LOW (0)") : F("HIGH (1)"));

  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, HIGH);
  tone(BUZZER_PIN, 1200, 100);

  int stepsToMove = (steppingMode == 8) ? (defaultStepsPerHalfTurn * 2) : defaultStepsPerHalfTurn;

  // 1. Rotate Forward 180°
  Serial.print(F(">>> [1/3] Rotating FORWARD "));
  Serial.print(stepsToMove);
  Serial.println(F(" steps (180°)..."));
  if (!stepMotor(stepsToMove, 1)) {
    Serial.println(F(">>> [ABORTED] Forward move interrupted by user STOP!"));
    releaseMotor();
    digitalWrite(LED_BLUE_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, HIGH);
    isExecutingCycle = false;
    return;
  }

  // 2. Pause briefly at drop position
  Serial.println(F(">>> [2/3] Holding at drop position for 500ms..."));
  for (int p = 0; p < 50; p++) {
    if (checkSerialBreak()) {
      releaseMotor();
      digitalWrite(LED_BLUE_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, HIGH);
      isExecutingCycle = false;
      return;
    }
    delay(10);
  }

  // 3. Rotate Reverse 180° back to home position
  Serial.print(F(">>> [3/3] Returning REVERSE "));
  Serial.print(stepsToMove);
  Serial.println(F(" steps back to starting position..."));
  if (!stepMotor(stepsToMove, -1)) {
    Serial.println(F(">>> [ABORTED] Return move interrupted by user STOP!"));
  } else {
    Serial.println(F(">>> [CYCLE COMPLETE] Successfully returned to start position!"));
    tone(BUZZER_PIN, 1800, 150);
  }

  // 4. Always de-energize coils at rest
  releaseMotor();
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
  isExecutingCycle = false;
  Serial.println(F(">>> Stepper coils de-energized. System READY."));
  Serial.println();
}

// ── HARDWARE ULN2003 DIAGNOSTIC TESTS ────────────────────────────────────────
void runUlnCoilCycleTest() {
  Serial.println(F("\n======================================================="));
  Serial.println(F("--- ULN2003 DRIVER BOARD LED WALKING TEST (3 CYCLES) ---"));
  Serial.println(F("Look directly at the 4 small red LEDs on the ULN2003 board:"));
  Serial.println(F("  LED A = Pin D3 (IN1)"));
  Serial.println(F("  LED B = Pin D4 (IN2)"));
  Serial.println(F("  LED C = Pin D11 (IN3)"));
  Serial.println(F("  LED D = Pin D12 (IN4)"));
  Serial.println(F("-------------------------------------------------------"));

  for (int round = 1; round <= 3; round++) {
    Serial.print(F("Cycle "));
    Serial.print(round);
    Serial.println(F(" of 3:"));

    // Coil 1
    releaseMotor();
    digitalWrite(pinIN1, HIGH);
    Serial.println(F("  [A] Pin IN1 (D3)  = HIGH ---> LED A should be ON"));
    delay(500);

    // Coil 2
    releaseMotor();
    digitalWrite(pinIN2, HIGH);
    Serial.println(F("  [B] Pin IN2 (D4)  = HIGH ---> LED B should be ON"));
    delay(500);

    // Coil 3
    releaseMotor();
    digitalWrite(pinIN3, HIGH);
    Serial.println(F("  [C] Pin IN3 (D11) = HIGH ---> LED C should be ON"));
    delay(500);

    // Coil 4
    releaseMotor();
    digitalWrite(pinIN4, HIGH);
    Serial.println(F("  [D] Pin IN4 (D12) = HIGH ---> LED D should be ON"));
    delay(500);
  }

  releaseMotor();
  Serial.println(F("-------------------------------------------------------"));
  Serial.println(F("DIAGNOSTIC RESULTS GUIDE:"));
  Serial.println(F("- If ALL 4 LEDs (A, B, C, D) lit up in order:"));
  Serial.println(F("    -> ULN2003 IC and Arduino output pins are physically 100% OK!"));
  Serial.println(F("    -> If the motor doesn't spin during STEP, check if motor is"));
  Serial.println(F("       plugged firmly into the white 5-pin socket or 5V power sag."));
  Serial.println(F("- If NONE of the LEDs lit up:"));
  Serial.println(F("    -> ULN2003 has NO POWER or NO COMMON GROUND with Uno!"));
  Serial.println(F("    -> Ensure ULN2003 GND is wired to Arduino Uno GND."));
  Serial.println(F("    -> Ensure ULN2003 '+' pin receives 5V. Check power jumper!"));
  Serial.println(F("- If only 1 or 2 LEDs lit up:"));
  Serial.println(F("    -> Check jumper wires on the non-lighting pins."));
  Serial.println(F("=======================================================\n"));
}

void testSingleCoil(int coilNum) {
  releaseMotor();
  Serial.print(F("Testing Single Coil #"));
  Serial.println(coilNum);
  switch (coilNum) {
    case 1:
      digitalWrite(pinIN1, HIGH);
      Serial.println(F("Coil 1 (Pin D3) is energized. LED A should be ON."));
      break;
    case 2:
      digitalWrite(pinIN2, HIGH);
      Serial.println(F("Coil 2 (Pin D4) is energized. LED B should be ON."));
      break;
    case 3:
      digitalWrite(pinIN3, HIGH);
      Serial.println(F("Coil 3 (Pin D11) is energized. LED C should be ON."));
      break;
    case 4:
      digitalWrite(pinIN4, HIGH);
      Serial.println(F("Coil 4 (Pin D12) is energized. LED D should be ON."));
      break;
    default:
      Serial.println(F("Invalid coil number! Choose 1, 2, 3, or 4."));
      break;
  }
  Serial.println(F("Send 'STOP' or 'RELEASE' to turn coil OFF."));
}

void testLedsAndBuzzer() {
  Serial.println(F("\n--- TESTING ONBOARD PERIPHERALS ---"));
  Serial.println(F("Blinking Green LED (D8)..."));
  digitalWrite(LED_GREEN_PIN, HIGH);
  delay(300);
  digitalWrite(LED_GREEN_PIN, LOW);

  Serial.println(F("Blinking Red LED (D9)..."));
  digitalWrite(LED_RED_PIN, HIGH);
  delay(300);
  digitalWrite(LED_RED_PIN, LOW);

  Serial.println(F("Blinking Blue LED (D13)..."));
  digitalWrite(LED_BLUE_PIN, HIGH);
  delay(300);
  digitalWrite(LED_BLUE_PIN, LOW);

  Serial.println(F("Sounding Buzzer (D10)..."));
  tone(BUZZER_PIN, 1500, 200);
  delay(250);
  tone(BUZZER_PIN, 2200, 200);
  delay(250);

  digitalWrite(LED_GREEN_PIN, HIGH);
  Serial.println(F("Peripheral test complete!\n"));
}

// ── STATUS DISPLAY ───────────────────────────────────────────────────────────
void printStatus() {
  int rawIr = digitalRead(IR_SENSOR_PIN);
  bool detected = isIrDetected();

  Serial.println(F("\n================ CURRENT HARDWARE STATUS ================"));
  Serial.print(F("IR Sensor Pin D7 : "));
  Serial.print(rawIr == LOW ? F("LOW (0)") : F("HIGH (1)"));
  Serial.print(F(" ---> "));
  Serial.println(detected ? F("[OBJECT DETECTED / BLOCKED]") : F("[CLEAR / NOTHING]"));
  Serial.print(F("IR Active Level  : "));
  Serial.println(irActiveLevel == LOW ? F("LOW (Active LOW - default)") : F("HIGH (Active HIGH)"));
  Serial.print(F("Auto-Trigger     : "));
  Serial.println(autoTriggerEnabled ? F("ENABLED (IR activates stepper)") : F("DISABLED (Manual only)"));
  Serial.print(F("ULN2003 Pins     : IN1=D"));
  Serial.print(pinIN1);
  Serial.print(F(", IN2=D"));
  Serial.print(pinIN2);
  Serial.print(F(", IN3=D"));
  Serial.print(pinIN3);
  Serial.print(F(", IN4=D"));
  Serial.println(pinIN4);
  Serial.print(F("Stepping Mode    : "));
  Serial.print(steppingMode);
  Serial.println(steppingMode == 8 ? F("-step (Half-step, 4096/rev)") : F("-step (Full-step, 2048/rev)"));
  Serial.print(F("Step Delay       : "));
  Serial.print(stepDelayMicros);
  Serial.println(F(" microseconds"));
  Serial.print(F("Continuous Move  : "));
  Serial.println(motorRunningContinuous ? F("RUNNING") : F("STOPPED"));
  Serial.println(F("=========================================================\n"));
}

// ── PRINT HELP MENU ──────────────────────────────────────────────────────────
void printHelp() {
  Serial.println(F("\n================ AVAILABLE SERIAL COMMANDS ================"));
  Serial.println(F("  TRIGGER / ROTATION COMMANDS:"));
  Serial.println(F("    180            - Run full 180° dispense test (forward + return)"));
  Serial.println(F("    360            - Rotate 1 full revolution (360°) forward"));
  Serial.println(F("    STEP [n]       - Step forward [n] steps (default 1024)"));
  Serial.println(F("    REV [n]        - Step reverse [n] steps (default 1024)"));
  Serial.println(F("    CONT           - Run stepper continuously forward"));
  Serial.println(F("    CONT REV       - Run stepper continuously in reverse"));
  Serial.println(F("    STOP (or OFF)  - Immediately stop motor & turn off all coils"));
  Serial.println(F("    RELEASE        - De-energize all stepper coils (all LOW)"));
  Serial.println(F(""));
  Serial.println(F("  HARDWARE DIAGNOSTIC COMMANDS:"));
  Serial.println(F("    CYCLE          - ULN2003 LED walking test (tests A->B->C->D LEDs)"));
  Serial.println(F("    COIL 1|2|3|4   - Energize a single coil to test with multimeter/LED"));
  Serial.println(F("    LEDS           - Test onboard Green/Red/Blue LEDs and Buzzer"));
  Serial.println(F("    STATUS         - Show current pin levels & sensor state"));
  Serial.println(F("    IR             - Read and display current IR sensor state"));
  Serial.println(F(""));
  Serial.println(F("  CONFIGURATION COMMANDS:"));
  Serial.println(F("    AUTO ON        - Enable automatic stepper activation on IR detect"));
  Serial.println(F("    AUTO OFF       - Disable automatic stepper activation on IR detect"));
  Serial.println(F("    IR_INV         - Invert IR trigger polarity (toggle LOW <-> HIGH)"));
  Serial.println(F("    SPEED [rpm]    - Adjust speed (e.g., SPEED 10, SPEED 15)"));
  Serial.println(F("    MODE 4|8       - Switch between 4-step and 8-step drive mode"));
  Serial.println(F("    REMAP 8 9 10 11- Remap IN1-IN4 pins on the fly if needed"));
  Serial.println(F("    HELP (or ?)    - Show this command list"));
  Serial.println(F("===========================================================\n"));
}

// ── SERIAL COMMAND PROCESSOR ─────────────────────────────────────────────────
void handleSerialCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  String upper = cmd;
  upper.toUpperCase();

  Serial.print(F("Command received: \""));
  Serial.print(cmd);
  Serial.println(F("\""));

  if (upper == "HELP" || upper == "?") {
    printHelp();
  }
  else if (upper == "STATUS") {
    printStatus();
  }
  else if (upper == "STOP" || upper == "OFF") {
    stopRequested = true;
    motorRunningContinuous = false;
    releaseMotor();
    digitalWrite(LED_BLUE_PIN, LOW);
    digitalWrite(LED_GREEN_PIN, HIGH);
    Serial.println(F("Motor stopped and coils released."));
  }
  else if (upper == "RELEASE") {
    releaseMotor();
    Serial.println(F("All stepper coils de-energized (LOW)."));
  }
  else if (upper == "180" || upper == "DISPENSE" || upper == "TEST") {
    runDispenseTestCycle("Serial '180' Command");
  }
  else if (upper == "360") {
    int stepsToMove = (steppingMode == 8) ? (defaultStepsPerHalfTurn * 4) : (defaultStepsPerHalfTurn * 2);
    Serial.print(F("Rotating full 360° ("));
    Serial.print(stepsToMove);
    Serial.println(F(" steps)..."));
    digitalWrite(LED_BLUE_PIN, HIGH);
    stepMotor(stepsToMove, 1);
    releaseMotor();
    digitalWrite(LED_BLUE_PIN, LOW);
    Serial.println(F("360° rotation complete. Coils released."));
  }
  else if (upper.startsWith("STEP")) {
    int steps = defaultStepsPerHalfTurn;
    int spaceIndex = upper.indexOf(' ');
    if (spaceIndex > 0) {
      steps = upper.substring(spaceIndex + 1).toInt();
      if (steps <= 0) steps = defaultStepsPerHalfTurn;
    }
    Serial.print(F("Stepping FORWARD "));
    Serial.print(steps);
    Serial.println(F(" steps..."));
    digitalWrite(LED_BLUE_PIN, HIGH);
    stepMotor(steps, 1);
    releaseMotor();
    digitalWrite(LED_BLUE_PIN, LOW);
    Serial.println(F("Step complete. Coils released."));
  }
  else if (upper.startsWith("REV")) {
    int steps = defaultStepsPerHalfTurn;
    int spaceIndex = upper.indexOf(' ');
    if (spaceIndex > 0) {
      steps = upper.substring(spaceIndex + 1).toInt();
      if (steps <= 0) steps = defaultStepsPerHalfTurn;
    }
    Serial.print(F("Stepping REVERSE "));
    Serial.print(steps);
    Serial.println(F(" steps..."));
    digitalWrite(LED_BLUE_PIN, HIGH);
    stepMotor(steps, -1);
    releaseMotor();
    digitalWrite(LED_BLUE_PIN, LOW);
    Serial.println(F("Reverse step complete. Coils released."));
  }
  else if (upper == "CONT") {
    Serial.println(F("Continuous FORWARD motion started. Type 'STOP' to halt."));
    motorRunningContinuous = true;
    continuousDirection = 1;
    digitalWrite(LED_BLUE_PIN, HIGH);
  }
  else if (upper == "CONT REV") {
    Serial.println(F("Continuous REVERSE motion started. Type 'STOP' to halt."));
    motorRunningContinuous = true;
    continuousDirection = -1;
    digitalWrite(LED_BLUE_PIN, HIGH);
  }
  else if (upper == "CYCLE") {
    runUlnCoilCycleTest();
  }
  else if (upper.startsWith("COIL")) {
    int spaceIndex = upper.indexOf(' ');
    if (spaceIndex > 0) {
      int coil = upper.substring(spaceIndex + 1).toInt();
      testSingleCoil(coil);
    } else {
      Serial.println(F("Usage: COIL 1, COIL 2, COIL 3, or COIL 4"));
    }
  }
  else if (upper == "LEDS") {
    testLedsAndBuzzer();
  }
  else if (upper == "IR") {
    int raw = digitalRead(IR_SENSOR_PIN);
    Serial.print(F("IR Pin D7 is "));
    Serial.print(raw == LOW ? F("LOW (0)") : F("HIGH (1)"));
    Serial.println(isIrDetected() ? F(" -> [OBJECT DETECTED]") : F(" -> [CLEAR]"));
  }
  else if (upper == "IR_INV") {
    irActiveLevel = (irActiveLevel == LOW) ? HIGH : LOW;
    Serial.print(F("IR active trigger level changed to: "));
    Serial.println(irActiveLevel == LOW ? F("LOW (0)") : F("HIGH (1)"));
  }
  else if (upper == "AUTO ON" || upper == "ON") {
    autoTriggerEnabled = true;
    Serial.println(F("AUTO-TRIGGER ENABLED: Stepper will activate when IR detects an object."));
  }
  else if (upper == "AUTO OFF") {
    autoTriggerEnabled = false;
    Serial.println(F("AUTO-TRIGGER DISABLED: Stepper will only move via manual commands."));
  }
  else if (upper.startsWith("SPEED")) {
    int spaceIndex = upper.indexOf(' ');
    if (spaceIndex > 0) {
      int rpm = upper.substring(spaceIndex + 1).toInt();
      if (rpm >= 1 && rpm <= 20) {
        // Approximate delay calculation for 2048 steps:
        // delayUs = (60 * 1,000,000) / (2048 * rpm)
        stepDelayMicros = (60000000UL) / (2048UL * (unsigned long)rpm);
        Serial.print(F("Speed set to "));
        Serial.print(rpm);
        Serial.print(F(" RPM (delay "));
        Serial.print(stepDelayMicros);
        Serial.println(F(" us)"));
      } else {
        Serial.println(F("Please choose RPM between 1 and 20 (e.g. SPEED 10)."));
      }
    }
  }
  else if (upper == "MODE 4") {
    steppingMode = 4;
    Serial.println(F("Stepping mode set to 4-step full stepping (2048 steps/360°)."));
  }
  else if (upper == "MODE 8") {
    steppingMode = 8;
    Serial.println(F("Stepping mode set to 8-step half stepping (4096 steps/360°)."));
  }
  else if (upper.startsWith("REMAP")) {
    // Allows user to remap pins on the fly: e.g. REMAP 8 9 10 11
    int p1, p2, p3, p4;
    int items = sscanf(cmd.c_str(), "%*s %d %d %d %d", &p1, &p2, &p3, &p4);
    if (items == 4) {
      pinIN1 = p1;
      pinIN2 = p2;
      pinIN3 = p3;
      pinIN4 = p4;
      pinMode(pinIN1, OUTPUT);
      pinMode(pinIN2, OUTPUT);
      pinMode(pinIN3, OUTPUT);
      pinMode(pinIN4, OUTPUT);
      releaseMotor();
      Serial.print(F("ULN2003 Pins remapped to: IN1=D"));
      Serial.print(pinIN1);
      Serial.print(F(", IN2=D"));
      Serial.print(pinIN2);
      Serial.print(F(", IN3=D"));
      Serial.print(pinIN3);
      Serial.print(F(", IN4=D"));
      Serial.println(pinIN4);
    } else {
      Serial.println(F("Usage: REMAP <in1> <in2> <in3> <in4>  (e.g., REMAP 3 4 11 12)"));
    }
  }
  else {
    Serial.println(F("Unknown command. Type 'HELP' or '?' for the command list."));
  }
}

// ── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);

  // Configure Pins
  pinMode(pinIN1, OUTPUT);
  pinMode(pinIN2, OUTPUT);
  pinMode(pinIN3, OUTPUT);
  pinMode(pinIN4, OUTPUT);

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Set initial safe states
  releaseMotor();
  digitalWrite(LED_GREEN_PIN, HIGH); // Green = READY
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);

  // Boot beep
  tone(BUZZER_PIN, 1200, 100);
  delay(120);
  tone(BUZZER_PIN, 1800, 150);

  Serial.println(F("\n==========================================================="));
  Serial.println(F("  BALLPEN UNO - ULN2003 STEPPER & IR HARDWARE TEST SKETCH  "));
  Serial.println(F("==========================================================="));
  Serial.println(F("Pin Connections:"));
  Serial.println(F("  ULN2003: IN1->D3, IN2->D4, IN3->D11, IN4->D12"));
  Serial.println(F("  IR Sensor: OUT->D7 (active LOW)"));
  Serial.println(F("  LEDs: Green->D8, Red->D9, Blue->D13 | Buzzer: D10"));
  Serial.println(F("-----------------------------------------------------------"));
  Serial.println(F("Feature: IR AUTO-TRIGGER IS ACTIVE!"));
  Serial.println(F("Cover the IR sensor with your hand to trigger the stepper!"));
  Serial.println(F("Type 'HELP' or '?' to see all diagnostic commands."));
  Serial.println(F("Type 'CYCLE' to test the ULN2003 board indicator LEDs."));
  Serial.println(F("-----------------------------------------------------------"));

  lastIrState = digitalRead(IR_SENSOR_PIN);
  printStatus();
}

// ── MAIN LOOP ────────────────────────────────────────────────────────────────
void loop() {
  // 1. Process Incoming Serial Commands
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    handleSerialCommand(cmd);
  }

  // 2. Handle Continuous Stepping (if triggered via CONT command)
  if (motorRunningContinuous) {
    stepOnce(continuousDirection);
    return;
  }

  // 3. Monitor IR Sensor
  int currentIr = digitalRead(IR_SENSOR_PIN);
  if (currentIr != lastIrState && (millis() - lastIrChangeTime > IR_DEBOUNCE_MS)) {
    lastIrState = currentIr;
    lastIrChangeTime = millis();

    bool detected = (currentIr == irActiveLevel);

    Serial.print(F("[IR EVENT] Sensor state changed: Pin D7 is "));
    Serial.print(currentIr == LOW ? F("LOW (0)") : F("HIGH (1)"));
    Serial.println(detected ? F(" ---> [OBJECT DETECTED]") : F(" ---> [CLEAR]"));

    if (detected) {
      if (autoTriggerEnabled && !isExecutingCycle) {
        Serial.println(F("\n>>> IR TRIGGER DETECTED! Activating Stepper Motor..."));
        runDispenseTestCycle("IR Drop Sensor (Pin D7)");
      }
    } else {
      digitalWrite(LED_GREEN_PIN, HIGH);
      digitalWrite(LED_BLUE_PIN, LOW);
    }
  }
}

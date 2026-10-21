/*
  Arduino Uno + TMC2209 + converted 28BYJ-48 + IR hardware test.

  Behavior:
    - An IR detection runs the test cycle: move forward, pause, return.
    - Serial Monitor commands can also move the motor and inspect the IR input.

  Wiring (UNO):
    TMC2209 STEP -> D3
    TMC2209 DIR  -> D4
    TMC2209 EN/ENN -> D11 (active LOW; this sketch drives LOW to enable)
    TMC2209 VIO -> UNO 5V if the carrier exposes/requires a separate logic supply.
    TMC2209 GND and UNO GND must be common; VM is the separate 12V motor rail.
    IR OUT -> D7, VCC -> suitable sensor supply, GND -> common GND.
    Green LED -> D8, Red LED -> D9, buzzer -> D10, Blue LED -> D13.

  Connect the converted motor's two isolated coil pairs to A1/A2 and B1/B2.
  Do not connect the motor's former common red wire. Confirm coil pairs with
  a meter and use the TMC carrier's own motor-supply requirements.

  IMPORTANT: MOVE_PULSES is a pulse count, not a calibrated angle. The angle
  depends on the TMC2209 MS1/MS2 microstep configuration and the motor gearbox.
  VREF/current calibration is hardware-only and is not changed by this sketch.
*/

#include <Arduino.h>

// TMC2209 control pins. D12 is intentionally no longer used as a motor output.
const uint8_t STEP_PIN = 3;
const uint8_t DIR_PIN = 4;
const uint8_t ENABLE_PIN = 11;

const uint8_t IR_SENSOR_PIN = 7;
const uint8_t LED_GREEN_PIN = 8;
const uint8_t LED_RED_PIN = 9;
const uint8_t BUZZER_PIN = 10;
const uint8_t LED_BLUE_PIN = 13;

const uint8_t STEP_PULSE_WIDTH_US = 5;
const int DEFAULT_MOVE_PULSES = 1024;
const unsigned long IR_DEBOUNCE_MS = 150;

unsigned long stepPeriodUs = 3000; // Initial conservative rate: about 333 pulses/s.
int movePulses = DEFAULT_MOVE_PULSES;
int irActiveLevel = LOW;
bool autoTriggerEnabled = true;
bool directionInverted = false;
bool motorRunningContinuous = false;
int continuousDirection = 1;
bool stopRequested = false;
bool isExecutingCycle = false;

int lastIrState = HIGH;
unsigned long lastIrChangeTime = 0;
String serialLine;

void disableDriver() {
  digitalWrite(ENABLE_PIN, HIGH);
  digitalWrite(STEP_PIN, LOW);
}

void enableDriver() {
  digitalWrite(ENABLE_PIN, LOW);
}

bool isIrDetected() {
  return digitalRead(IR_SENSOR_PIN) == irActiveLevel;
}

// While moving, accept STOP/OFF immediately without blocking on Serial timeout.
bool pollStopCommand() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialLine.trim();
      serialLine.toUpperCase();
      bool stop = serialLine == "STOP" || serialLine == "OFF";
      serialLine = "";
      if (stop) {
        stopRequested = true;
        motorRunningContinuous = false;
        disableDriver();
        Serial.println(F("STOP received; driver disabled."));
        return true;
      }
      continue;
    }
    if (serialLine.length() < 48) serialLine += c;
  }
  return stopRequested;
}

void emitStepPulse() {
  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(STEP_PULSE_WIDTH_US);
  digitalWrite(STEP_PIN, LOW);
  if (stepPeriodUs > STEP_PULSE_WIDTH_US) {
    delayMicroseconds(stepPeriodUs - STEP_PULSE_WIDTH_US);
  }
}

bool moveMotor(long pulses, int direction) {
  if (pulses <= 0) return true;
  digitalWrite(DIR_PIN, ((direction > 0) != directionInverted) ? HIGH : LOW);
  for (long i = 0; i < pulses; ++i) {
    if (pollStopCommand()) return false;
    emitStepPulse();
  }
  return true;
}

void finishMotion() {
  disableDriver();
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

void runDispenseTestCycle(const char *triggerSource) {
  if (isExecutingCycle) return;
  isExecutingCycle = true;
  stopRequested = false;

  Serial.println();
  Serial.print(F("IR test cycle triggered by: "));
  Serial.println(triggerSource);
  Serial.print(F("Moving "));
  Serial.print(movePulses);
  Serial.println(F(" STEP pulses forward, then the same count back."));
  Serial.println(F("This pulse count is not a guaranteed angle; it depends on MS1/MS2 microstepping."));

  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, HIGH);
  tone(BUZZER_PIN, 1200, 100);
  enableDriver();

  if (!moveMotor(movePulses, 1)) {
    Serial.println(F("Cycle stopped during forward movement."));
    finishMotion();
    isExecutingCycle = false;
    return;
  }

  Serial.println(F("At end position; holding 500 ms."));
  for (uint8_t i = 0; i < 50; ++i) {
    if (pollStopCommand()) {
      Serial.println(F("Cycle stopped during pause."));
      finishMotion();
      isExecutingCycle = false;
      return;
    }
    delay(10);
  }

  if (moveMotor(movePulses, -1)) {
    Serial.println(F("IR test cycle complete; returned by the same pulse count."));
    tone(BUZZER_PIN, 1800, 150);
  } else {
    Serial.println(F("Cycle stopped during return movement."));
  }
  finishMotion();
  isExecutingCycle = false;
  Serial.println();
}

void testIndicators() {
  Serial.println(F("Testing LEDs and buzzer."));
  digitalWrite(LED_GREEN_PIN, HIGH);
  tone(BUZZER_PIN, 1000, 150);
  delay(250);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, HIGH);
  tone(BUZZER_PIN, 1400, 150);
  delay(250);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, HIGH);
  tone(BUZZER_PIN, 1800, 150);
  delay(250);
  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, HIGH);
}

void printStatus() {
  Serial.println(F("--- TMC2209 + IR test status ---"));
  Serial.print(F("STEP D3: ")); Serial.println(digitalRead(STEP_PIN));
  Serial.print(F("DIR D4: ")); Serial.println(digitalRead(DIR_PIN));
  Serial.print(F("EN D11: "));
  Serial.println(digitalRead(ENABLE_PIN) == LOW ? F("LOW (enabled)") : F("HIGH (disabled)"));
  Serial.print(F("IR D7: "));
  Serial.print(digitalRead(IR_SENSOR_PIN) == LOW ? F("LOW") : F("HIGH"));
  Serial.println(isIrDetected() ? F(" (detected)") : F(" (clear)"));
  Serial.print(F("Auto trigger: ")); Serial.println(autoTriggerEnabled ? F("ON") : F("OFF"));
  Serial.print(F("Move pulses each way: ")); Serial.println(movePulses);
  Serial.print(F("Pulse rate: ")); Serial.print(1000000UL / stepPeriodUs); Serial.println(F(" pulses/s"));
  Serial.print(F("Direction inverted: ")); Serial.println(directionInverted ? F("YES") : F("NO"));
}

void printHelp() {
  Serial.println(F("\nTMC2209 + IR commands (9600 baud, Newline):"));
  Serial.println(F("  180 / TEST       IR-style forward, pause, return cycle"));
  Serial.println(F("  STEP [n]         Move forward up to 80000 pulses"));
  Serial.println(F("  REV [n]          Move reverse n pulses"));
  Serial.println(F("  360              Move 2x configured pulses forward (not calibrated degrees)"));
  Serial.println(F("  CONT [REV]       Continuous stepping; send STOP to halt"));
  Serial.println(F("  STOP / OFF       Stop motion and disable the driver"));
  Serial.println(F("  RELEASE          Disable driver outputs"));
  Serial.println(F("  ENABLE           Enable driver outputs (motor may hold position)"));
  Serial.println(F("  COUNT [n]        Set pulse count used by TEST (1..20000)"));
  Serial.println(F("  SPEED [n]        Set pulse rate in pulses/s (50..15000)"));
  Serial.println(F("  AUTO ON|OFF      Enable/disable IR-triggered test cycle"));
  Serial.println(F("  IR               Read current IR state"));
  Serial.println(F("  IR_INV           Toggle IR active level LOW/HIGH"));
  Serial.println(F("  DIR_INV          Toggle direction polarity"));
  Serial.println(F("  STATUS           Show pin and test configuration"));
  Serial.println(F("  LEDS             Test indicators and buzzer"));
  Serial.println(F("  HELP / ?         Show this list\n"));
}

long commandNumber(const String &upper, long fallback) {
  int space = upper.indexOf(' ');
  if (space < 0) return fallback;
  long value = upper.substring(space + 1).toInt();
  return value > 0 ? value : fallback;
}

void handleSerialCommand(String cmd) {
  cmd.trim();
  if (!cmd.length()) return;
  String upper = cmd;
  upper.toUpperCase();
  Serial.print(F("Command: "));
  Serial.println(cmd);

  if (upper == "HELP" || upper == "?") printHelp();
  else if (upper == "STATUS") printStatus();
  else if (upper == "IR") {
    Serial.print(F("IR D7 is "));
    Serial.print(digitalRead(IR_SENSOR_PIN) == LOW ? F("LOW") : F("HIGH"));
    Serial.println(isIrDetected() ? F(" -> DETECTED") : F(" -> CLEAR"));
  }
  else if (upper == "STOP" || upper == "OFF") {
    stopRequested = true;
    motorRunningContinuous = false;
    finishMotion();
    Serial.println(F("Motor stopped; driver disabled."));
  }
  else if (upper == "RELEASE" || upper == "DISABLE") {
    motorRunningContinuous = false;
    disableDriver();
    Serial.println(F("TMC2209 driver disabled."));
  }
  else if (upper == "ENABLE") {
    enableDriver();
    Serial.println(F("TMC2209 driver enabled; motor may hold position."));
  }
  else if (upper == "180" || upper == "TEST" || upper == "DISPENSE") {
    runDispenseTestCycle("Serial command");
  }
  else if (upper == "360") {
    stopRequested = false;
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, HIGH);
    enableDriver();
    bool completed = moveMotor((long)movePulses * 2L, 1);
    finishMotion();
    Serial.println(completed ? F("360 test pulse move complete.") : F("Move stopped."));
  }
  else if (upper.startsWith("STEP") || upper.startsWith("REV")) {
    bool reverse = upper.startsWith("REV");
    long pulses = commandNumber(upper, movePulses);
    if (pulses > 80000) pulses = 80000;
    stopRequested = false;
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, HIGH);
    enableDriver();
    bool completed = moveMotor(pulses, reverse ? -1 : 1);
    finishMotion();
    Serial.println(completed ? F("Move complete; driver disabled.") : F("Move stopped."));
  }
  else if (upper == "CONT" || upper == "CONT REV") {
    stopRequested = false;
    continuousDirection = upper == "CONT REV" ? -1 : 1;
    motorRunningContinuous = true;
    digitalWrite(LED_GREEN_PIN, LOW);
    digitalWrite(LED_BLUE_PIN, HIGH);
    enableDriver();
    Serial.println(F("Continuous stepping. Send STOP and newline to stop."));
  }
  else if (upper.startsWith("COUNT")) {
    long count = commandNumber(upper, movePulses);
    if (count >= 1 && count <= 20000) {
      movePulses = (int)count;
      Serial.print(F("Test cycle count set to "));
      Serial.print(movePulses);
      Serial.println(F(" STEP pulses each way."));
    } else Serial.println(F("Use COUNT 1..20000."));
  }
  else if (upper.startsWith("SPEED")) {
    long rate = commandNumber(upper, 0);
    if (rate >= 50 && rate <= 15000) {
      stepPeriodUs = 1000000UL / (unsigned long)rate;
      Serial.print(F("Pulse rate set to approximately "));
      Serial.print(1000000UL / stepPeriodUs);
      Serial.println(F(" pulses/s. Increase gradually and watch for missed steps."));
    } else Serial.println(F("Use SPEED 50..15000 (step pulses per second, not RPM)."));
  }
  else if (upper == "AUTO ON" || upper == "ON") {
    autoTriggerEnabled = true;
    Serial.println(F("IR auto-trigger enabled."));
  }
  else if (upper == "AUTO OFF") {
    autoTriggerEnabled = false;
    Serial.println(F("IR auto-trigger disabled."));
  }
  else if (upper == "IR_INV") {
    irActiveLevel = irActiveLevel == LOW ? HIGH : LOW;
    Serial.print(F("IR active level is now "));
    Serial.println(irActiveLevel == LOW ? F("LOW") : F("HIGH"));
  }
  else if (upper == "DIR_INV") {
    directionInverted = !directionInverted;
    Serial.print(F("Direction polarity inverted: "));
    Serial.println(directionInverted ? F("YES") : F("NO"));
  }
  else if (upper == "LEDS") testIndicators();
  else Serial.println(F("Unknown command. Type HELP."));
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(50);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  disableDriver();
  digitalWrite(LED_GREEN_PIN, HIGH);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_BLUE_PIN, LOW);

  tone(BUZZER_PIN, 1200, 100);
  delay(120);
  tone(BUZZER_PIN, 1800, 150);

  Serial.println(F("\nUNO + TMC2209 + converted 28BYJ-48 IR test"));
  Serial.println(F("STEP=D3, DIR=D4, EN/ENN=D11 (active LOW), IR=D7"));
  Serial.println(F("Power TMC logic VIO as required by the carrier; share UNO/driver GND."));
  Serial.println(F("Indicators: green D8, red D9, buzzer D10, blue D13"));
  Serial.println(F("IR auto-trigger is ON: detection runs forward/pause/return."));
  Serial.println(F("Initial rate is about 333 pulses/s; test movement count is pulses, not degrees."));
  Serial.println(F("Driver starts disabled. Type HELP for commands."));

  lastIrState = digitalRead(IR_SENSOR_PIN);
  printStatus();
}

void loop() {
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    handleSerialCommand(cmd);
  }

  if (motorRunningContinuous) {
    if (pollStopCommand()) {
      finishMotion();
      return;
    }
    emitStepPulse();
    return;
  }

  int currentIr = digitalRead(IR_SENSOR_PIN);
  if (currentIr != lastIrState && millis() - lastIrChangeTime > IR_DEBOUNCE_MS) {
    lastIrState = currentIr;
    lastIrChangeTime = millis();
    bool detected = currentIr == irActiveLevel;
    Serial.print(F("IR D7 changed to "));
    Serial.println(currentIr == LOW ? F("LOW") : F("HIGH"));
    if (detected) {
      Serial.println(F("Object detected."));
      if (autoTriggerEnabled) runDispenseTestCycle("IR sensor D7");
    } else {
      digitalWrite(LED_GREEN_PIN, HIGH);
      digitalWrite(LED_BLUE_PIN, LOW);
    }
  }
}

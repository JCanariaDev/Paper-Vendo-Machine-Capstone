/*
  Hardware-only bench test for the first (Black) pen compartment and P1 hopper.

  This sketch has NO ESP32, Wi-Fi, database, display, or sales logic.

  Required wiring:
    Black pen ULN2003: IN1=D3, IN2=D11, IN3=D4, IN4=D12
    Black pen IR OUT:  D7   (expected HIGH clear, LOW when pen passes)
    Hopper relay IN:   D8   (default assumes relay activates on HIGH)
    Hopper coin OUT:   D2   (expected HIGH idle, LOW when a P1 coin passes)

  Open Serial Monitor at 115200 baud and send one command:
    HELP         show commands
    STATUS       show sensor states
    PEN          run one complete Black-pen dispense cycle and verify IR
    JOG 200      move pen stepper forward 200 steps
    JOG -200     move pen stepper backward 200 steps
    HOPPER 1     release/count one coin; stops after sensor confirmation
    HOPPER 5     release/count five coins; stops after five confirmations
    HOPPER ON    run hopper continuously (maximum 10 seconds safety stop)
    HOPPER OFF   immediately stop hopper

  Safety:
    - Test with hopper empty first, then add only P1 coins.
    - Hopper must be powered through a relay/motor driver; never from D8.
    - If relay turns ON at boot, set HOPPER_RELAY_ON to LOW below.
*/

#include <Stepper.h>

const int PEN_STEPS_PER_REVOLUTION = 2048;
const int PEN_IR_PIN = 7;
const int HOPPER_RELAY_PIN = 8;
const int HOPPER_SENSOR_PIN = 2;

// Change this to LOW only if your relay is confirmed active-low.
const int HOPPER_RELAY_ON = HIGH;
const int HOPPER_RELAY_OFF = (HOPPER_RELAY_ON == HIGH) ? LOW : HIGH;

const unsigned long PEN_SENSOR_TIMEOUT_MS = 5000;
const unsigned long HOPPER_COIN_TIMEOUT_MS = 5000;
const unsigned long HOPPER_CONTINUOUS_MAX_MS = 10000;

// Keep this argument order exactly matched to the existing Black pen wiring.
Stepper blackPenStepper(PEN_STEPS_PER_REVOLUTION, 3, 11, 4, 12);
const int penStopPins[4] = { 3, 4, 11, 12 };

bool hopperContinuous = false;
unsigned long hopperContinuousStartedAt = 0;

void stopPenStepper() {
  for (int i = 0; i < 4; i++) digitalWrite(penStopPins[i], LOW);
}

void hopperOff() {
  digitalWrite(HOPPER_RELAY_PIN, HOPPER_RELAY_OFF);
  hopperContinuous = false;
  Serial.println("Hopper: OFF");
}

void hopperOn() {
  digitalWrite(HOPPER_RELAY_PIN, HOPPER_RELAY_ON);
  Serial.println("Hopper: ON");
}

void printStatus() {
  Serial.print("Pen IR D7: ");
  Serial.println(digitalRead(PEN_IR_PIN) == LOW ? "LOW (blocked/detected)" : "HIGH (clear)");
  Serial.print("Hopper coin OUT D2: ");
  Serial.println(digitalRead(HOPPER_SENSOR_PIN) == LOW ? "LOW (coin/switch active)" : "HIGH (idle)");
}

bool waitForPenDrop() {
  unsigned long startedAt = millis();
  while (millis() - startedAt < PEN_SENSOR_TIMEOUT_MS) {
    if (digitalRead(PEN_IR_PIN) == LOW) return true;
  }
  return false;
}

void testOnePen() {
  if (digitalRead(PEN_IR_PIN) == LOW) {
    Serial.println("PEN ABORT: IR is already LOW. Clear/align the sensor first.");
    return;
  }

  Serial.println("Pen: moving to drop position...");
  blackPenStepper.step(1024);
  bool detected = waitForPenDrop();

  Serial.println("Pen: returning to home position...");
  blackPenStepper.step(-1024);
  stopPenStepper();

  Serial.println(detected ? "PEN PASS: IR confirmed a falling pen." : "PEN FAIL: no IR detection within 5 seconds.");
}

bool releaseCoins(int expectedCoins) {
  if (expectedCoins <= 0) return false;
  if (digitalRead(HOPPER_SENSOR_PIN) == LOW) {
    Serial.println("HOPPER ABORT: sensor is already LOW. Clear/verify the sensor first.");
    return false;
  }

  int countedCoins = 0;
  bool previousLow = false;
  unsigned long lastCoinAt = millis();
  hopperOn();

  while (countedCoins < expectedCoins && millis() - lastCoinAt < HOPPER_COIN_TIMEOUT_MS) {
    bool low = digitalRead(HOPPER_SENSOR_PIN) == LOW;
    if (low && !previousLow) {
      countedCoins++;
      lastCoinAt = millis();
      Serial.print("Coin detected: ");
      Serial.println(countedCoins);
    }
    previousLow = low;
  }

  hopperOff();
  if (countedCoins == expectedCoins) {
    Serial.println("HOPPER PASS: requested coin count was sensor-confirmed.");
    return true;
  }

  Serial.print("HOPPER FAIL: expected ");
  Serial.print(expectedCoins);
  Serial.print(", detected ");
  Serial.println(countedCoins);
  return false;
}

void printHelp() {
  Serial.println();
  Serial.println("Commands: HELP | STATUS | PEN | JOG <steps> | HOPPER <count> | HOPPER ON | HOPPER OFF");
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "HELP") printHelp();
  else if (command == "STATUS") printStatus();
  else if (command == "PEN") testOnePen();
  else if (command.startsWith("JOG ")) {
    int steps = command.substring(4).toInt();
    if (steps == 0) Serial.println("JOG FAIL: use a non-zero number, e.g. JOG 200");
    else {
      Serial.print("Jogging pen stepper: "); Serial.println(steps);
      blackPenStepper.step(steps);
      stopPenStepper();
    }
  }
  else if (command == "HOPPER ON") {
    hopperOn();
    hopperContinuous = true;
    hopperContinuousStartedAt = millis();
    Serial.println("Safety stop: 10 seconds.");
  }
  else if (command == "HOPPER OFF") hopperOff();
  else if (command.startsWith("HOPPER ")) {
    int count = command.substring(7).toInt();
    if (count <= 0) Serial.println("HOPPER FAIL: use HOPPER 1, HOPPER 2, etc.");
    else releaseCoins(count);
  }
  else if (command.length()) {
    Serial.println("Unknown command.");
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);
  blackPenStepper.setSpeed(10);

  pinMode(PEN_IR_PIN, INPUT_PULLUP);
  pinMode(HOPPER_SENSOR_PIN, INPUT_PULLUP);
  pinMode(HOPPER_RELAY_PIN, OUTPUT);
  hopperOff();
  stopPenStepper();

  Serial.println("=== PEN + HOPPER HARDWARE TEST ===");
  Serial.println("Hopper starts OFF. Check that the relay/hopper is not running.");
  printStatus();
  printHelp();
}

void loop() {
  if (Serial.available()) {
    handleCommand(Serial.readStringUntil('\n'));
  }

  if (hopperContinuous && millis() - hopperContinuousStartedAt >= HOPPER_CONTINUOUS_MAX_MS) {
    Serial.println("Safety timeout reached.");
    hopperOff();
  }
}

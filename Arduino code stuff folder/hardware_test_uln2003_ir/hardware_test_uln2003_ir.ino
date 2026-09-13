#include <Stepper.h>

/*
  ULN2003 + 28BYJ-48 + IR SENSOR TEST

  This standalone test uses the same Mega pins as the main sketch:
    ULN2003 IN1 -> D3
    ULN2003 IN2 -> D4
    ULN2003 IN3 -> D11
    ULN2003 IN4 -> D12
    IR sensor OUT -> D7

  IR behavior matches the main sketch:
    LOW  = detected / beam broken
    HIGH = nothing detected

  Serial commands at 115200 baud:
    ON  - start the test
    OFF - stop immediately and de-energize the motor coils
*/

const int STEPS_PER_REVOLUTION = 2048;
const int HALF_TURN_STEPS = STEPS_PER_REVOLUTION / 2;
const int MOTOR_SPEED_RPM = 10;

const int STEPPER_IN1_PIN = 3;
const int STEPPER_IN2_PIN = 4;
const int STEPPER_IN3_PIN = 11;
const int STEPPER_IN4_PIN = 12;
const int IR_SENSOR_PIN = 7;

const unsigned long SENSOR_WAIT_MS = 3000;
const unsigned int MAX_HALF_TURNS = 20;

Stepper testStepper(
  STEPS_PER_REVOLUTION,
  STEPPER_IN1_PIN,
  STEPPER_IN3_PIN,
  STEPPER_IN2_PIN,
  STEPPER_IN4_PIN
);

bool testRunning = false;
bool stopRequested = false;

void stopStepper() {
  digitalWrite(STEPPER_IN1_PIN, LOW);
  digitalWrite(STEPPER_IN2_PIN, LOW);
  digitalWrite(STEPPER_IN3_PIN, LOW);
  digitalWrite(STEPPER_IN4_PIN, LOW);
}

bool sensorDetected() {
  return digitalRead(IR_SENSOR_PIN) == LOW;
}

void handleSerialCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "OFF") {
    stopRequested = true;
    if (!testRunning) {
      stopStepper();
      Serial.println(F("OFF: stepper is already stopped."));
    }
    return;
  }

  if (command == "ON") {
    if (testRunning) {
      Serial.println(F("Test is already running. Enter OFF to stop it."));
    } else {
      stopRequested = false;
      testRunning = true;
    }
    return;
  }

  if (command.length() > 0) {
    Serial.println(F("Unknown command. Enter ON or OFF."));
  }
}

bool checkForStopCommand() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }

  if (stopRequested) {
    stopStepper();
    testRunning = false;
    Serial.println(F("OFF: stepper stopped at its current position."));
    return true;
  }
  return false;
}

bool moveStepsInterruptible(int steps) {
  const int direction = steps >= 0 ? 1 : -1;
  const int numberOfSteps = abs(steps);

  for (int step = 0; step < numberOfSteps; step++) {
    if (checkForStopCommand()) return false;
    testStepper.step(direction);
  }
  return true;
}

// Wait while checking both the IR sensor and the serial port.
// Returns true when the sensor detects something.
bool waitForSensorOrStop() {
  const unsigned long startedAt = millis();
  while (millis() - startedAt < SENSOR_WAIT_MS) {
    if (checkForStopCommand()) return false;
    if (sensorDetected()) return true;
    delay(5);
  }
  return sensorDetected();
}

void runStepperTest() {
  long stepsAwayFromStart = 0;
  unsigned int halfTurns = 0;

  Serial.println();
  Serial.println(F("=== ULN2003 STEPPER TEST STARTED ==="));
  Serial.println(F("Each move is 180 degrees, followed by a 3-second sensor check."));

  while (testRunning && halfTurns < MAX_HALF_TURNS) {
    Serial.print(F("Moving 180 degrees. Test turn: "));
    Serial.println(halfTurns + 1);

    if (!moveStepsInterruptible(HALF_TURN_STEPS)) return;
    stepsAwayFromStart += HALF_TURN_STEPS;
    halfTurns++;

    Serial.println(F("Stopped for sensor check."));
    if (waitForSensorOrStop()) {
      Serial.println(F("Detected"));
      Serial.println(F("Returning to the starting position."));
      moveStepsInterruptible(-stepsAwayFromStart);
      stopStepper();
      testRunning = false;
      Serial.println(F("Test complete. Stepper OFF at starting position."));
      return;
    }

    if (!testRunning) return;
    Serial.println(F("Nothing"));
    Serial.println(F("No signal detected. Running another 180-degree test move."));
  }

  if (testRunning) {
    Serial.println(F("Maximum test rotations reached without detection."));
    Serial.println(F("Returning to the starting position."));
    moveStepsInterruptible(-stepsAwayFromStart);
    stopStepper();
    testRunning = false;
    Serial.println(F("Test complete. Stepper OFF at starting position."));
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(50);

  pinMode(IR_SENSOR_PIN, INPUT_PULLUP);
  pinMode(STEPPER_IN1_PIN, OUTPUT);
  pinMode(STEPPER_IN2_PIN, OUTPUT);
  pinMode(STEPPER_IN3_PIN, OUTPUT);
  pinMode(STEPPER_IN4_PIN, OUTPUT);

  testStepper.setSpeed(MOTOR_SPEED_RPM);
  stopStepper();

  Serial.println(F("ULN2003 + IR sensor test ready."));
  Serial.println(F("Enter ON to start or OFF to stop."));
  Serial.println(sensorDetected() ? F("Initial sensor state: Detected")
                                  : F("Initial sensor state: Nothing"));
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    handleSerialCommand(command);
  }

  if (testRunning) {
    runStepperTest();
  }
}

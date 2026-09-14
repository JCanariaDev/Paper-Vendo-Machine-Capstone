#include <Stepper.h>

/*
  UNO BALLPEN CONTROLLER TEST

  UART communication:
    Uno D0 / RX <- Mega D14 / TX3
    Uno D1 / TX -> Mega D15 / RX3
    Uno GND     <-> Mega GND
    9600 baud

  Ballpen hardware on this Uno:
    ULN2003 IN1 -> D3
    ULN2003 IN2 -> D4
    ULN2003 IN3 -> D11
    ULN2003 IN4 -> D12
    Ballpen IR OUT -> D7 (active LOW)
    Green LED -> D8
    Red LED   -> D9
    Blue LED  -> D13
    Passive buzzer -> D10

  The motor must use a suitable external 5V supply through the ULN2003.
  Connect the external-supply GND to the Uno GND.
*/

const int STEPS_PER_REVOLUTION = 2048;
const int HALF_TURN_STEPS = STEPS_PER_REVOLUTION / 2;
const int MOTOR_SPEED_RPM = 10;

const int STEPPER_IN1_PIN = 3;
const int STEPPER_IN2_PIN = 4;
const int STEPPER_IN3_PIN = 11;
const int STEPPER_IN4_PIN = 12;
const int BALLPEN_IR_PIN = 7;
const int LED_GREEN_PIN = 8;
const int LED_RED_PIN = 9;
const int BUZZER_PIN = 10;
const int LED_BLUE_PIN = 13;

const unsigned long SENSOR_WAIT_MS = 3000;

Stepper ballpenStepper(
  STEPS_PER_REVOLUTION,
  STEPPER_IN1_PIN,
  STEPPER_IN3_PIN,
  STEPPER_IN2_PIN,
  STEPPER_IN4_PIN
);

bool stopRequested = false;
bool testRunning = false;

bool sensorDetected() {
  return digitalRead(BALLPEN_IR_PIN) == LOW;
}

void disableMotor() {
  digitalWrite(STEPPER_IN1_PIN, LOW);
  digitalWrite(STEPPER_IN2_PIN, LOW);
  digitalWrite(STEPPER_IN3_PIN, LOW);
  digitalWrite(STEPPER_IN4_PIN, LOW);
}

void setRunningIndicators(bool running) {
  digitalWrite(LED_GREEN_PIN, running ? LOW : HIGH);
  digitalWrite(LED_BLUE_PIN, running ? HIGH : LOW);
  digitalWrite(LED_RED_PIN, LOW);
}

void handleCommand(const String &command) {
  if (command == "BALLPEN_STOP") {
    stopRequested = true;
    disableMotor();
    setRunningIndicators(false);
    noTone(BUZZER_PIN);
    Serial.println(F("BALLPEN_STOPPED"));
  }
}

bool checkForStopCommand() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    handleCommand(command);
  }

  if (stopRequested) {
    testRunning = false;
    disableMotor();
    return true;
  }
  return false;
}

bool moveInterruptible(int steps) {
  const int direction = steps >= 0 ? 1 : -1;
  const int numberOfSteps = abs(steps);

  for (int step = 0; step < numberOfSteps; step++) {
    if (checkForStopCommand()) return false;
    ballpenStepper.step(direction);
  }
  return true;
}

bool waitForSensorOrStop() {
  const unsigned long startedAt = millis();
  while (millis() - startedAt < SENSOR_WAIT_MS) {
    if (checkForStopCommand()) return false;
    if (sensorDetected()) return true;
    delay(5);
  }
  return sensorDetected();
}

void runBallpenTest() {
  stopRequested = false;
  testRunning = true;
  setRunningIndicators(true);
  tone(BUZZER_PIN, 1100, 120);

  Serial.println(F("BALLPEN_TEST_STARTED"));
  if (!moveInterruptible(HALF_TURN_STEPS)) return;

  bool detected = waitForSensorOrStop();
  if (!testRunning) return;

  if (detected) {
    Serial.println(F("BALLPEN_RESULT:DETECTED"));
    digitalWrite(LED_GREEN_PIN, HIGH);
    tone(BUZZER_PIN, 1800, 150);
  } else {
    Serial.println(F("BALLPEN_RESULT:NOTHING"));
    digitalWrite(LED_RED_PIN, HIGH);
  }

  // Always return to the starting position after the sensor check.
  moveInterruptible(-HALF_TURN_STEPS);
  disableMotor();
  setRunningIndicators(false);
  testRunning = false;
  Serial.println(F("BALLPEN_TEST_COMPLETE"));
}

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(100);

  pinMode(STEPPER_IN1_PIN, OUTPUT);
  pinMode(STEPPER_IN2_PIN, OUTPUT);
  pinMode(STEPPER_IN3_PIN, OUTPUT);
  pinMode(STEPPER_IN4_PIN, OUTPUT);
  pinMode(BALLPEN_IR_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  ballpenStepper.setSpeed(MOTOR_SPEED_RPM);
  disableMotor();
  setRunningIndicators(false);

  Serial.println(F("UNO_BALLPEN_READY"));
  Serial.println(sensorDetected() ? F("IR:DETECTED") : F("IR:NOTHING"));
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "BALLPEN_TEST" && !testRunning) {
      runBallpenTest();
    } else {
      handleCommand(command);
    }
  }
}

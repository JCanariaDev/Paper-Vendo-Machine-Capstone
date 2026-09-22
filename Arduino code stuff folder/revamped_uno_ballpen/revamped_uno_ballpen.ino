#include <Stepper.h>

/*
  ARDUINO UNO - BALLPEN CONTROLLER
  Receives dispense commands from the Mega over D0/D1.

  UART:
    Uno D0/RX <- Mega D14/TX3
    Uno D1/TX -> Mega D15/RX3

  Hardware:
    ULN2003 IN1/IN2/IN3/IN4 -> D3/D4/D11/D12
    Ballpen IR OUT -> D7 (active LOW)
    Green LED -> D8, red LED -> D9, buzzer -> D10, blue LED -> D13
*/

const int STEPS_PER_REVOLUTION = 2048;
const int HALF_TURN_STEPS = STEPS_PER_REVOLUTION / 2;
const int MOTOR_SPEED_RPM = 10;
const int SENSOR_WAIT_MS = 3000;

const int STEPPER_IN1_PIN = 3;
const int STEPPER_IN2_PIN = 4;
const int STEPPER_IN3_PIN = 11;
const int STEPPER_IN4_PIN = 12;
const int BALLPEN_IR_PIN = 7;
const int LED_GREEN_PIN = 8;
const int LED_RED_PIN = 9;
const int BUZZER_PIN = 10;
const int LED_BLUE_PIN = 13;

Stepper ballpenStepper(
  STEPS_PER_REVOLUTION,
  STEPPER_IN1_PIN,
  STEPPER_IN3_PIN,
  STEPPER_IN2_PIN,
  STEPPER_IN4_PIN
);

bool stopRequested = false;
bool dispensing = false;

bool sensorDetected() {
  return digitalRead(BALLPEN_IR_PIN) == LOW;
}

void disableMotor() {
  digitalWrite(STEPPER_IN1_PIN, LOW);
  digitalWrite(STEPPER_IN2_PIN, LOW);
  digitalWrite(STEPPER_IN3_PIN, LOW);
  digitalWrite(STEPPER_IN4_PIN, LOW);
}

void setIndicator(const String &state) {
  digitalWrite(LED_GREEN_PIN, state == "READY" ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN, state == "ACTIVE" ? HIGH : LOW);
  digitalWrite(LED_RED_PIN, state == "ERROR" ? HIGH : LOW);
}

void stopBallpen() {
  stopRequested = true;
  disableMotor();
  noTone(BUZZER_PIN);
  setIndicator("READY");
}

bool readStopCommand() {
  while (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "STOP" || command == "BALLPEN_STOP") {
      stopBallpen();
      return true;
    }
  }
  return stopRequested;
}

bool moveInterruptible(int steps) {
  const int direction = steps >= 0 ? 1 : -1;
  const int count = abs(steps);
  for (int i = 0; i < count; i++) {
    if (readStopCommand()) return false;
    ballpenStepper.step(direction);
  }
  return true;
}

bool waitForSensor() {
  unsigned long startedAt = millis();
  while (millis() - startedAt < SENSOR_WAIT_MS) {
    if (readStopCommand()) return false;
    if (sensorDetected()) return true;
    delay(5);
  }
  return sensorDetected();
}

bool dispenseOnePen() {
  if (sensorDetected()) return false;
  if (!moveInterruptible(HALF_TURN_STEPS)) return false;

  bool detected = waitForSensor();
  if (stopRequested) return false;

  delay(100); // Let the pen clear the sensor, without delaying the next item.
  bool returned = moveInterruptible(-HALF_TURN_STEPS);
  disableMotor();
  return detected && returned;
}

void dispensePens(int channel, int quantity) {
  if (channel != 1 || quantity <= 0) {
    Serial.println("BALLPEN_FAIL:" + String(channel) + ":0:BAD_REQUEST");
    return;
  }

  stopRequested = false;
  dispensing = true;
  setIndicator("ACTIVE");
  tone(BUZZER_PIN, 1100, 120);

  int dispensed = 0;
  for (int i = 0; i < quantity; i++) {
    if (!dispenseOnePen()) break;
    dispensed++;
  }

  disableMotor();
  dispensing = false;
  if (dispensed == quantity) {
    setIndicator("READY");
    tone(BUZZER_PIN, 1800, 150);
    Serial.println("BALLPEN_DONE:" + String(channel) + ":" + String(dispensed));
  } else {
    setIndicator("ERROR");
    Serial.println("BALLPEN_FAIL:" + String(channel) + ":" + String(dispensed) + ":" +
                   (stopRequested ? "STOPPED" : "IR_TIMEOUT"));
    stopRequested = false;
  }
}

void handleCommand(String command) {
  command.trim();
  if (command == "STOP" || command == "BALLPEN_STOP") {
    stopBallpen();
    Serial.println("BALLPEN_STOPPED");
  }
  else if (command == "STATUS?") {
    Serial.println("BALLPEN_READY");
    Serial.println(sensorDetected() ? "IR:DETECTED" : "IR:NOTHING");
  }
  else if (command.startsWith("INDICATOR:")) {
    setIndicator(command.substring(10));
  }
  else if (command.startsWith("BEEP:")) {
    int first = command.indexOf(':');
    int second = command.indexOf(':', first + 1);
    if (first > 0 && second > first) {
      tone(BUZZER_PIN, command.substring(first + 1, second).toInt(),
           command.substring(second + 1).toInt());
    }
  }
  else if (command.startsWith("DISPENSE:")) {
    int first = command.indexOf(':');
    int second = command.indexOf(':', first + 1);
    if (first > 0 && second > first) {
      dispensePens(command.substring(first + 1, second).toInt(),
                   command.substring(second + 1).toInt());
    }
  }
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
  setIndicator("READY");
  Serial.println("BALLPEN_READY");
}

void loop() {
  if (Serial.available() > 0) {
    handleCommand(Serial.readStringUntil('\n'));
  }
}

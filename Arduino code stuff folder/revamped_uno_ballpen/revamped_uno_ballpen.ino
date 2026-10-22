#include <Stepper.h>

/*
  ARDUINO UNO - BALLPEN CONTROLLER
  Receives dispense commands from the Mega over D0/D1.

  UART:
    Uno D0/RX <- Mega D14/TX3
    Uno D1/TX -> Mega D15/RX3

  Hardware:
    ULN2003 IN1/IN2/IN3/IN4 -> D3/D4/D11/D12
    ULN2003 motor supply -> voltage rated for the 28BYJ-48 (typically 5V)
    ULN2003 GND and Uno GND must be common
    Ballpen IR OUT -> D7 (active LOW)
    Green LED -> D8, red LED -> D9, buzzer -> D10, blue LED -> D13
*/

// Starting calibration for the 28BYJ-48 through ULN2003; verify on the mechanism.
const int STEPS_PER_REVOLUTION = 2048;
const int HALF_TURN_STEPS = 1024;
const int MOTOR_SPEED_RPM = 10;
const int SENSOR_WAIT_MS = 3000;
const int BALLPEN_IR_TRIGGER_LEVEL = LOW; // LOW when object detected. Change to HIGH if your sensor outputs HIGH when covered

const int MOTOR_IN1_PIN = 3;
const int MOTOR_IN2_PIN = 4;
const int MOTOR_IN3_PIN = 11;
const int MOTOR_IN4_PIN = 12;
const int BALLPEN_IR_PIN = 7;
const int LED_GREEN_PIN = 8;
const int LED_RED_PIN = 9;
const int BUZZER_PIN = 10;
const int LED_BLUE_PIN = 13;

Stepper ballpenStepper(STEPS_PER_REVOLUTION,
                       MOTOR_IN1_PIN, MOTOR_IN3_PIN,
                       MOTOR_IN2_PIN, MOTOR_IN4_PIN);

bool stopRequested = false;
bool dispensing = false;

bool sensorDetected() {
  return digitalRead(BALLPEN_IR_PIN) == BALLPEN_IR_TRIGGER_LEVEL;
}

void disableMotor() {
  digitalWrite(MOTOR_IN1_PIN, LOW);
  digitalWrite(MOTOR_IN2_PIN, LOW);
  digitalWrite(MOTOR_IN3_PIN, LOW);
  digitalWrite(MOTOR_IN4_PIN, LOW);
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

bool moveInterruptible(long steps, bool *dropFlag = nullptr, int baseline = -1) {
  const long count = steps >= 0 ? steps : -steps;
  const int direction = steps >= 0 ? 1 : -1;

  for (long i = 0; i < count; i++) {
    if (readStopCommand()) return false;
    ballpenStepper.step(direction);
    if (dropFlag != nullptr) {
      if (digitalRead(BALLPEN_IR_PIN) == BALLPEN_IR_TRIGGER_LEVEL ||
          (baseline >= 0 && digitalRead(BALLPEN_IR_PIN) != baseline)) {
        *dropFlag = true;
      }
    }
  }
  return true;
}

bool waitForSensorDrop(int baseline) {
  unsigned long startedAt = millis();
  while (millis() - startedAt < SENSOR_WAIT_MS) {
    if (readStopCommand()) return false;
    if (digitalRead(BALLPEN_IR_PIN) == BALLPEN_IR_TRIGGER_LEVEL ||
        (baseline >= 0 && digitalRead(BALLPEN_IR_PIN) != baseline)) {
      return true;
    }
    delay(5);
  }
  return false;
}

bool dispenseOnePen() {
  bool penDetected = false;
  const int baseline = digitalRead(BALLPEN_IR_PIN);

  Serial.print(F("DISPENSE: Start. Raw IR Pin D7 = "));
  Serial.print(baseline == LOW ? F("LOW (0)") : F("HIGH (1)"));
  Serial.println(F(". Rotating half-turn forward to drop position..."));

  // 1. Rotate the calibrated half-turn forward to the drop position.
  bool forwardCompleted = moveInterruptible(HALF_TURN_STEPS, &penDetected, baseline);
  if (!forwardCompleted) {
    // If stop command received, still return to start
    moveInterruptible(-HALF_TURN_STEPS);
    disableMotor();
    return false;
  }

  // 2. STOP and wait for IR sensor if not detected during forward movement
  if (!penDetected) {
    Serial.println(F("DISPENSE: Holding at drop position. Waiting for pen drop on IR sensor..."));
    penDetected = waitForSensorDrop(baseline);
  }

  if (penDetected) {
    Serial.println(F("DISPENSE: Pen detected by IR sensor!"));
  } else {
    Serial.println(F("DISPENSE: IR timeout - no pen detected."));
  }

  delay(200); // Brief pause before returning

  // 3. Return by the same calibrated half-turn in reverse.
  Serial.println(F("DISPENSE: Reversing half-turn back to original starting position..."));
  bool returned = moveInterruptible(-HALF_TURN_STEPS);

  // 4. De-energize coils so motor stays cool at home position
  disableMotor();

  if (!returned) {
    Serial.println(F("DISPENSE: Return move interrupted!"));
    return false;
  }

  Serial.println(penDetected ? F("DISPENSE: SUCCESS - Returned to start position.")
                             : F("DISPENSE: FAILED (IR timeout) - Returned to start position."));
  return penDetected;
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
  else if (command == "IR" || command == "IR?") {
    Serial.print("IR_STATUS: Pin D7 is ");
    Serial.println(sensorDetected() ? "BLOCKED/LOW" : "CLEAR/HIGH");
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

  pinMode(MOTOR_IN1_PIN, OUTPUT);
  pinMode(MOTOR_IN2_PIN, OUTPUT);
  pinMode(MOTOR_IN3_PIN, OUTPUT);
  pinMode(MOTOR_IN4_PIN, OUTPUT);
  pinMode(BALLPEN_IR_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  ballpenStepper.setSpeed(MOTOR_SPEED_RPM);
  disableMotor();
  setIndicator("READY");
  tone(BUZZER_PIN, 1200, 100);
  delay(130);
  tone(BUZZER_PIN, 1800, 150);
  Serial.println("BALLPEN_READY");
  Serial.print("BALLPEN_BOOT: IR Sensor Pin D7 is ");
  Serial.println(sensorDetected() ? "BLOCKED/LOW" : "CLEAR/HIGH");
}

void loop() {
  if (Serial.available() > 0) {
    handleCommand(Serial.readStringUntil('\n'));
  }
}

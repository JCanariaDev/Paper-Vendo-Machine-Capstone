/*
  Hardware-only bench test for four NEMA17 stepper motors with TMC2209 drivers.

  Intended use:
    Paper dispensing mechanism test on Arduino Uno.

  This sketch uses simple STEP/DIR mode.
  It does NOT use UART configuration, ESP32, Wi-Fi, display, database, or sales logic.

  Required wiring for Arduino Uno:
    Paper motor 1 TMC2209 STEP: D2
    Paper motor 1 TMC2209 DIR:  D3
    Paper motor 2 TMC2209 STEP: D4
    Paper motor 2 TMC2209 DIR:  D5
    Paper motor 3 TMC2209 STEP: D6
    Paper motor 3 TMC2209 DIR:  D7
    Paper motor 4 TMC2209 STEP: D8
    Paper motor 4 TMC2209 DIR:  D9
    Paper motor 1 TMC2209 EN:   D10  (active LOW on most TMC2209 modules)
    Paper motor 2 TMC2209 EN:   D11
    Paper motor 3 TMC2209 EN:   D12
    Paper motor 4 TMC2209 EN:   D13

    All TMC2209 VIO/5V pins: Arduino 5V, if your module exposes logic power
    All TMC2209 GND pins: Arduino GND and motor power supply GND
    All TMC2209 VMOT/VM pins: external motor supply +, commonly 12V
    NEMA17 coils: one coil pair to A1/A2, the other coil pair to B1/B2

  Open Serial Monitor at 115200 baud and send one command:
    HELP             show commands
    STATUS           show current settings and pinout
    ENABLE 1         enable motor 1 driver
    ENABLE ALL       enable all drivers
    DISABLE 1        disable motor 1 driver
    DISABLE ALL      disable all drivers
    JOG 1 200        move motor 1 forward 200 steps
    JOG 2 -200       move motor 2 backward 200 steps
    REV 3            move motor 3 one 200-step revolution
    ALL 200          move all four motors forward 200 steps, one after another
    SPEED 900        set step pulse delay in microseconds
    DIR 4 CW         set motor 4 default direction clockwise
    DIR 4 CCW        set motor 4 default direction counter-clockwise
    DIR ALL CW       set all default directions clockwise

  Safety:
    - Do not connect/disconnect motors while VMOT is powered.
    - Set each TMC2209 current limit/Vref before long tests.
    - Use an external motor supply; do not power NEMA17 motors from the Arduino.
    - Put an electrolytic capacitor across VMOT/GND near each driver if possible.
    - If a motor only vibrates, its coil pairs are probably wired incorrectly.
*/

const int MOTOR_COUNT = 4;

const int STEP_PINS[MOTOR_COUNT] = {2, 4, 6, 8};
const int DIR_PINS[MOTOR_COUNT] = {3, 5, 7, 9};
const int ENABLE_PINS[MOTOR_COUNT] = {10, 11, 12, 13};

const int DRIVER_ENABLE_ON = LOW;
const int DRIVER_ENABLE_OFF = HIGH;

const int FULL_STEPS_PER_REVOLUTION = 200;
const unsigned int MIN_STEP_DELAY_US = 300;
const unsigned int MAX_STEP_DELAY_US = 5000;

unsigned int stepDelayUs = 900;
bool defaultDirectionClockwise[MOTOR_COUNT] = {true, true, true, true};
bool driverEnabled[MOTOR_COUNT] = {false, false, false, false};

void enableDriver(int motorIndex) {
  if (!isValidMotorIndex(motorIndex)) {
    Serial.println("ENABLE FAIL: motor must be 1 to 4.");
    return;
  }

  digitalWrite(ENABLE_PINS[motorIndex], DRIVER_ENABLE_ON);
  driverEnabled[motorIndex] = true;
  Serial.print("Motor ");
  Serial.print(motorIndex + 1);
  Serial.println(" driver: ENABLED");
}

void disableDriver(int motorIndex) {
  if (!isValidMotorIndex(motorIndex)) {
    Serial.println("DISABLE FAIL: motor must be 1 to 4.");
    return;
  }

  digitalWrite(ENABLE_PINS[motorIndex], DRIVER_ENABLE_OFF);
  driverEnabled[motorIndex] = false;
  Serial.print("Motor ");
  Serial.print(motorIndex + 1);
  Serial.println(" driver: DISABLED");
}

bool isValidMotorIndex(int motorIndex) {
  return motorIndex >= 0 && motorIndex < MOTOR_COUNT;
}

void enableAllDrivers() {
  for (int i = 0; i < MOTOR_COUNT; i++) {
    enableDriver(i);
  }
}

void disableAllDrivers() {
  for (int i = 0; i < MOTOR_COUNT; i++) {
    disableDriver(i);
  }
}

void setDirection(int motorIndex, bool clockwise) {
  if (!isValidMotorIndex(motorIndex)) return;
  defaultDirectionClockwise[motorIndex] = clockwise;
  digitalWrite(DIR_PINS[motorIndex], clockwise ? HIGH : LOW);
}

void pulseStep(int motorIndex) {
  digitalWrite(STEP_PINS[motorIndex], HIGH);
  delayMicroseconds(stepDelayUs);
  digitalWrite(STEP_PINS[motorIndex], LOW);
  delayMicroseconds(stepDelayUs);
}

void moveMotorSteps(int motorIndex, long steps) {
  if (!isValidMotorIndex(motorIndex)) {
    Serial.println("MOVE FAIL: motor must be 1 to 4.");
    return;
  }

  if (steps == 0) {
    Serial.println("MOVE FAIL: step count is zero.");
    return;
  }

  bool moveClockwise = steps > 0 ? defaultDirectionClockwise[motorIndex] : !defaultDirectionClockwise[motorIndex];
  long totalSteps = labs(steps);

  setDirection(motorIndex, moveClockwise);
  enableDriver(motorIndex);

  Serial.print("Motor ");
  Serial.print(motorIndex + 1);
  Serial.print(" moving steps: ");
  Serial.println(steps);

  for (long i = 0; i < totalSteps; i++) {
    pulseStep(motorIndex);
  }

  Serial.println("MOVE DONE");
  disableDriver(motorIndex);
}

void moveAllMotors(long steps) {
  if (steps == 0) {
    Serial.println("ALL FAIL: step count is zero.");
    return;
  }

  for (int i = 0; i < MOTOR_COUNT; i++) {
    moveMotorSteps(i, steps);
    delay(250);
  }

  Serial.println("ALL DONE");
}

void printStatus() {
  Serial.println();
  Serial.println("=== FOUR NEMA17 + TMC2209 STATUS ===");
  Serial.print("Step delay: "); Serial.print(stepDelayUs); Serial.println(" us");
  Serial.print("Full-step revolution setting: ");
  Serial.print(FULL_STEPS_PER_REVOLUTION);
  Serial.println(" steps");

  for (int i = 0; i < MOTOR_COUNT; i++) {
    Serial.print("Motor ");
    Serial.print(i + 1);
    Serial.print(": STEP D");
    Serial.print(STEP_PINS[i]);
    Serial.print(", DIR D");
    Serial.print(DIR_PINS[i]);
    Serial.print(", EN D");
    Serial.print(ENABLE_PINS[i]);
    Serial.print(", driver ");
    Serial.print(driverEnabled[i] ? "ENABLED" : "DISABLED");
    Serial.print(", default ");
    Serial.println(defaultDirectionClockwise[i] ? "CW" : "CCW");
  }
}

void printHelp() {
  Serial.println();
  Serial.println("Commands:");
  Serial.println("  HELP | STATUS");
  Serial.println("  ENABLE <motor 1-4> | ENABLE ALL");
  Serial.println("  DISABLE <motor 1-4> | DISABLE ALL");
  Serial.println("  JOG <motor 1-4> <steps>");
  Serial.println("  REV <motor 1-4>");
  Serial.println("  ALL <steps>");
  Serial.println("  SPEED <microseconds>");
  Serial.println("  DIR <motor 1-4> CW");
  Serial.println("  DIR <motor 1-4> CCW");
  Serial.println("  DIR ALL CW");
  Serial.println("  DIR ALL CCW");
}

int readSecondTokenAsInt(String command, int firstSpace) {
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  String token = secondSpace < 0 ? command.substring(firstSpace + 1) : command.substring(firstSpace + 1, secondSpace);
  return token.toInt();
}

long readThirdTokenAsLong(String command, int firstSpace) {
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  if (secondSpace < 0) return 0;
  return command.substring(secondSpace + 1).toInt();
}

void handleDirectionCommand(String command) {
  int firstSpace = command.indexOf(' ');
  int secondSpace = command.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0) {
    Serial.println("DIR FAIL: use DIR 1 CW or DIR ALL CCW.");
    return;
  }

  String target = command.substring(firstSpace + 1, secondSpace);
  String direction = command.substring(secondSpace + 1);
  bool clockwise = direction == "CW";

  if (direction != "CW" && direction != "CCW") {
    Serial.println("DIR FAIL: direction must be CW or CCW.");
    return;
  }

  if (target == "ALL") {
    for (int i = 0; i < MOTOR_COUNT; i++) setDirection(i, clockwise);
    Serial.print("All default directions set to ");
    Serial.println(clockwise ? "CW." : "CCW.");
    return;
  }

  int motorIndex = target.toInt() - 1;
  if (!isValidMotorIndex(motorIndex)) {
    Serial.println("DIR FAIL: motor must be 1 to 4.");
    return;
  }

  setDirection(motorIndex, clockwise);
  Serial.print("Motor ");
  Serial.print(motorIndex + 1);
  Serial.print(" default direction set to ");
  Serial.println(clockwise ? "CW." : "CCW.");
}

void handleEnableCommand(String command, bool enable) {
  int firstSpace = command.indexOf(' ');
  if (firstSpace < 0) {
    Serial.println(enable ? "ENABLE FAIL: use ENABLE 1 or ENABLE ALL." : "DISABLE FAIL: use DISABLE 1 or DISABLE ALL.");
    return;
  }

  String target = command.substring(firstSpace + 1);
  if (target == "ALL") {
    if (enable) enableAllDrivers();
    else disableAllDrivers();
    return;
  }

  int motorIndex = target.toInt() - 1;
  if (enable) enableDriver(motorIndex);
  else disableDriver(motorIndex);
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();

  if (command == "HELP") printHelp();
  else if (command == "STATUS") printStatus();
  else if (command.startsWith("ENABLE ")) handleEnableCommand(command, true);
  else if (command.startsWith("DISABLE ")) handleEnableCommand(command, false);
  else if (command.startsWith("DIR ")) handleDirectionCommand(command);
  else if (command.startsWith("JOG ")) {
    int firstSpace = command.indexOf(' ');
    int motorIndex = readSecondTokenAsInt(command, firstSpace) - 1;
    long steps = readThirdTokenAsLong(command, firstSpace);
    moveMotorSteps(motorIndex, steps);
  }
  else if (command.startsWith("REV ")) {
    int motorIndex = command.substring(4).toInt() - 1;
    moveMotorSteps(motorIndex, FULL_STEPS_PER_REVOLUTION);
  }
  else if (command.startsWith("ALL ")) {
    long steps = command.substring(4).toInt();
    moveAllMotors(steps);
  }
  else if (command.startsWith("SPEED ")) {
    int requestedDelay = command.substring(6).toInt();
    if (requestedDelay < MIN_STEP_DELAY_US || requestedDelay > MAX_STEP_DELAY_US) {
      Serial.print("SPEED FAIL: use ");
      Serial.print(MIN_STEP_DELAY_US);
      Serial.print(" to ");
      Serial.print(MAX_STEP_DELAY_US);
      Serial.println(" microseconds.");
    } else {
      stepDelayUs = requestedDelay;
      Serial.print("Step delay set to ");
      Serial.print(stepDelayUs);
      Serial.println(" us.");
    }
  }
  else if (command.length()) {
    Serial.println("Unknown command.");
    printHelp();
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < MOTOR_COUNT; i++) {
    pinMode(STEP_PINS[i], OUTPUT);
    pinMode(DIR_PINS[i], OUTPUT);
    pinMode(ENABLE_PINS[i], OUTPUT);
    digitalWrite(STEP_PINS[i], LOW);
    setDirection(i, defaultDirectionClockwise[i]);
    disableDriver(i);
  }

  Serial.println("=== FOUR NEMA17 + TMC2209 PAPER HARDWARE TEST ===");
  Serial.println("Drivers start DISABLED and auto-disable after every move.");
  printStatus();
  printHelp();
}

void loop() {
  if (Serial.available()) {
    handleCommand(Serial.readStringUntil('\n'));
  }
}

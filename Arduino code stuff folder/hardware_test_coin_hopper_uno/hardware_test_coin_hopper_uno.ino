/*
  ==============================================================================
  ALLAN 220V AC COIN HOPPER TEST FIRMWARE (ARDUINO UNO)
  ------------------------------------------------------------------------------
  Designed to safely test and calibrate the 220V AC Coin Hopper:
  - Switches 220V AC motor on/off via 5V Relay Module
  - Counts coin exit pulses via the Green PCB IR Sensor
  - Features real-time Serial Monitor telemetry & timeout safety protections
  ==============================================================================

  WIRING GUIDE FOR ARDUINO UNO:
  ------------------------------------------------------------------------------
  1. RELAY MODULE (Low Voltage Side):
     - VCC  --> Arduino Uno 5V
     - GND  --> Arduino Uno GND
     - IN   --> Arduino Uno Pin D7 (HOPPER_RELAY_PIN)

  2. GREEN PCB SENSOR BOARD:
     - VCC (Red)    --> Arduino Uno 5V (or 12V if sensor board requires 12V)
     - GND (Black)  --> Arduino Uno GND (Must share common ground with Arduino)
     - SIG (Yellow) --> Arduino Uno Pin D2 (HOPPER_SENSOR_PIN, INT0)

  3. 220V AC MOTOR (High Voltage Side - CAUTION!):
     - 220V AC Neutral Wire --> Direct to Hopper AC Motor Neutral Terminal
     - 220V AC Live Wire    --> [2A Fuse] --> Relay COM
     - Relay NO (Normally Open) --> Hopper AC Motor Live Terminal
  ==============================================================================
*/

// --- PIN CONFIGURATION ---
const int HOPPER_RELAY_PIN  = 7; // Controls the 5V Relay Module (SRD-05VDC-SL-C)
const int HOPPER_SENSOR_PIN = 2; // Green PCB IR Sensor Signal (Interrupt 0)

// --- RELAY POLARITY CONFIGURATION ---
// Set to 0 if your relay is Active-HIGH (HIGH = ON, LOW = OFF)
// Set to 1 if your relay is Active-LOW  (LOW = ON, HIGH = OFF)
// We provide an automatic software toggle 'INVERT' in Serial Monitor
int relayActiveLow = 1; // Default Active-LOW

int getRelayOnLevel()  { return relayActiveLow ? LOW : HIGH; }
int getRelayOffLevel() { return relayActiveLow ? HIGH : LOW; }

// --- SAFETY TIMEOUTS ---
const unsigned long COIN_TIMEOUT_MS = 4000; // Shuts off if no coin passes within 4s (empty/jam protection)
const unsigned long DEBOUNCE_MS     = 30;   // Optical sensor pulse debounce threshold

// --- RUNTIME STATE ---
volatile int coinsDispensedCount = 0;
volatile unsigned long lastPulseTime = 0;
volatile bool newCoinPulseFlag = false;

volatile bool isDispensing = false;
int targetCoinCount = 3; // Default 3 coins
unsigned long dispenseStartedAt = 0;
unsigned long lastCoinDispensedAt = 0;

void turnMotorOn() {
  digitalWrite(HOPPER_RELAY_PIN, getRelayOnLevel());
}

void turnMotorOff() {
  digitalWrite(HOPPER_RELAY_PIN, getRelayOffLevel());
}

void coinSensorISR() {
  unsigned long now = millis();
  // Filter out electrical double-bounces (30ms threshold)
  if (now - lastPulseTime > DEBOUNCE_MS) {
    coinsDispensedCount++;
    lastPulseTime = now;
    newCoinPulseFlag = true;

    // IMMEDIATE HARD CUTOFF inside interrupt
    if (isDispensing && coinsDispensedCount >= targetCoinCount) {
      isDispensing = false;
      digitalWrite(HOPPER_RELAY_PIN, getRelayOffLevel()); // Cut power instantly!
    }
  }
}

void printMenu() {
  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("   ALLAN 220V AC COIN HOPPER TEST - ARDUINO UNO + RELAY   "));
  Serial.println(F("=========================================================="));
  Serial.print(F(" Current Relay Mode: "));
  Serial.println(relayActiveLow ? F("Active-LOW (Standard)") : F("Active-HIGH"));
  Serial.println(F(" Commands you can type in Serial Monitor:"));
  Serial.println(F("   3           -> Dispense 3 coins"));
  Serial.println(F("   <number>    -> Dispense custom number of coins (e.g. 5, 10)"));
  Serial.println(F("   ON          -> Manually turn Relay/Motor ON"));
  Serial.println(F("   OFF         -> Manually turn Relay/Motor OFF (CUT POWER)"));
  Serial.println(F("   INVERT      -> Flip Relay ON/OFF logic (if relay is inverted)"));
  Serial.println(F("   SENSOR      -> Check live Green PCB Sensor state"));
  Serial.println(F("   HELP        -> Print this menu"));
  Serial.println(F("=========================================================="));
  Serial.println();
}

void startDispense(int target) {
  if (target <= 0) {
    Serial.println(F("ERROR: Target coins must be greater than 0!"));
    return;
  }

  targetCoinCount = target;
  noInterrupts();
  coinsDispensedCount = 0;
  newCoinPulseFlag = false;
  isDispensing = true;
  interrupts();

  dispenseStartedAt = millis();
  lastCoinDispensedAt = millis();

  Serial.println();
  Serial.print(F(">>> STARTING DISPENSE: Target = "));
  Serial.print(targetCoinCount);
  Serial.println(F(" Coins <<<"));
  Serial.println(F("Relay ON -> Motor spinning..."));

  turnMotorOn();
}

void checkSensorState() {
  int rawState = digitalRead(HOPPER_SENSOR_PIN);
  Serial.println();
  Serial.println(F("--- SENSOR DIAGNOSTIC ---"));
  Serial.print(F("Pin D2 Level: "));
  if (rawState == HIGH) {
    Serial.println(F("HIGH (Beam Unbroken / Clear / Ready)"));
  } else {
    Serial.println(F("LOW (Beam Broken / Coin Detected / Blocked)"));
  }
  Serial.println(F("Tip: Pass a coin through the sensor slot to verify it changes to LOW."));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // Relay initialization (ensure OFF state immediately on power up)
  pinMode(HOPPER_RELAY_PIN, OUTPUT);
  turnMotorOff();

  // Sensor initialization with internal pullup
  pinMode(HOPPER_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HOPPER_SENSOR_PIN), coinSensorISR, FALLING);

  printMenu();

  // Automatic 3-coin test countdown
  Serial.println(F(">>> AUTOMATIC TEST: Starting 3-coin dispense in 3 seconds... <<<"));
  delay(1000);
  Serial.println(F(" 2..."));
  delay(1000);
  Serial.println(F(" 1..."));
  delay(1000);

  // Auto-start dispensing 3 coins
  startDispense(3);
}

void loop() {
  unsigned long now = millis();

  // 1. Process Real-time Coin Pulses
  if (newCoinPulseFlag) {
    newCoinPulseFlag = false;
    lastCoinDispensedAt = now;
    unsigned long elapsed = now - dispenseStartedAt;

    Serial.print(F(" [COIN PULSE #"));
    Serial.print(coinsDispensedCount);
    Serial.print(F(" of "));
    Serial.print(targetCoinCount);
    Serial.print(F("] at +"));
    Serial.print(elapsed);
    Serial.println(F(" ms"));
  }

  // 2. Check Dispense Completion
  if (isDispensing) {
    if (coinsDispensedCount >= targetCoinCount) {
      turnMotorOff();
      isDispensing = false;
      unsigned long totalDuration = now - dispenseStartedAt;

      Serial.println();
      Serial.println(F("=================================================="));
      Serial.println(F("  SUCCESS! Target coins reached. Motor STOPPED.   "));
      Serial.print(F("  Total Coins Dispensed: "));
      Serial.println(coinsDispensedCount);
      Serial.print(F("  Total Time Taken:      "));
      Serial.print(totalDuration / 1000.0, 2);
      Serial.println(F(" seconds"));
      Serial.print(F("  Dispense Rate:         "));
      Serial.print(coinsDispensedCount / (totalDuration / 1000.0), 2);
      Serial.println(F(" coins/sec"));
      Serial.println(F("=================================================="));
      Serial.println();
    }
    // 3. Safety Timeout (Hopper empty or jammed)
    else if (now - lastCoinDispensedAt >= COIN_TIMEOUT_MS) {
      turnMotorOff();
      isDispensing = false;

      Serial.println();
      Serial.println(F("**************************************************"));
      Serial.println(F("  SAFETY TIMEOUT: No coin detected for 4 seconds! "));
      Serial.println(F("  Motor automatically STOPPED to prevent burnout. "));
      Serial.print(F("  Coins dispensed before timeout: "));
      Serial.print(coinsDispensedCount);
      Serial.print(F(" of "));
      Serial.println(targetCoinCount);
      Serial.println(F("**************************************************"));
      Serial.println();
    }
  }

  // 4. Handle Serial Monitor User Input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() == 0) return;

    if (input == "HELP" || input == "?") {
      printMenu();
    }
    else if (input == "ON") {
      Serial.println(F("MANUAL OVERRIDE: Turning Motor ON..."));
      isDispensing = false;
      turnMotorOn();
    }
    else if (input == "OFF" || input == "STOP") {
      Serial.println(F("MANUAL OVERRIDE: Turning Motor OFF."));
      isDispensing = false;
      turnMotorOff();
    }
    else if (input == "INVERT") {
      relayActiveLow = !relayActiveLow;
      turnMotorOff();
      Serial.println();
      Serial.print(F(">>> RELAY LOGIC INVERTED! Current Mode: "));
      Serial.println(relayActiveLow ? F("Active-LOW") : F("Active-HIGH"));
      Serial.println(F("Motor set to OFF with new polarity."));
      Serial.println();
    }
    else if (input == "SENSOR" || input == "TEST") {
      checkSensorState();
    }
    else if (input.toInt() > 0) {
      int count = input.toInt();
      startDispense(count);
    }
    else {
      Serial.print(F("Unknown command: '"));
      Serial.print(input);
      Serial.println(F("'. Type 3 to dispense or INVERT to flip relay logic."));
    }
  }
}

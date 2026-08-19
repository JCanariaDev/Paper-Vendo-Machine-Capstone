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
// Most standard SRD-05VDC-SL-C blue relay modules with optocoupler are Active-LOW:
// LOW  (0V) = Relay ON  (Motor Runs)
// HIGH (5V) = Relay OFF (Motor Stops)
#define RELAY_ACTIVE_LOW 1

#if RELAY_ACTIVE_LOW
  const int RELAY_ON  = LOW;
  const int RELAY_OFF = HIGH;
#else
  const int RELAY_ON  = HIGH;
  const int RELAY_OFF = LOW;
#endif

// --- SAFETY TIMEOUTS ---
const unsigned long COIN_TIMEOUT_MS = 3500; // Shuts off if no coin passes within 3.5s (empty/jam protection)
const unsigned long DEBOUNCE_MS     = 40;   // Optical sensor pulse debounce threshold

// --- RUNTIME STATE ---
volatile int coinsDispensedCount = 0;
volatile unsigned long lastPulseTime = 0;
volatile bool newCoinPulseFlag = false;

bool isDispensing = false;
int targetCoinCount = 3; // Default 3 coins
unsigned long dispenseStartedAt = 0;
unsigned long lastCoinDispensedAt = 0;

void coinSensorISR() {
  unsigned long now = millis();
  // Filter out electrical double-bounces
  if (now - lastPulseTime > DEBOUNCE_MS) {
    coinsDispensedCount++;
    lastPulseTime = now;
    newCoinPulseFlag = true;

    // INSTANT MOTOR SHUTOFF inside ISR for microsecond precision stopping
    if (isDispensing && coinsDispensedCount >= targetCoinCount) {
      digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF); // Cut power immediately!
    }
  }
}

void printMenu() {
  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("   ALLAN 220V AC COIN HOPPER TEST - ARDUINO UNO + RELAY   "));
  Serial.println(F("=========================================================="));
  Serial.println(F(" Commands you can type in Serial Monitor:"));
  Serial.println(F("   3           -> Dispense 3 coins (Default test)"));
  Serial.println(F("   5 or 10     -> Dispense custom number of coins"));
  Serial.println(F("   ON          -> Manually turn Relay/Motor ON"));
  Serial.println(F("   OFF         -> Manually turn Relay/Motor OFF"));
  Serial.println(F("   SENSOR      -> Check live Green PCB Sensor state"));
  Serial.println(F("   HELP        -> Print this menu"));
  Serial.println(F("=========================================================="));
  Serial.println(F("Type a command and press ENTER (or type 3 to test):"));
  Serial.println();
}

void turnMotorOn() {
  digitalWrite(HOPPER_RELAY_PIN, RELAY_ON);
}

void turnMotorOff() {
  digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF);
}

void startDispense(int target) {
  if (target <= 0) {
    Serial.println(F("ERROR: Target coins must be greater than 0!"));
    return;
  }

  // Pre-check sensor state
  if (digitalRead(HOPPER_SENSOR_PIN) == LOW) {
    Serial.println(F("WARNING: Coin sensor is currently BLOCKED (LOW) before starting!"));
    Serial.println(F("Please check if a coin is stuck in the exit chute or if sensor is misaligned."));
    return;
  }

  targetCoinCount = target;
  noInterrupts();
  coinsDispensedCount = 0;
  newCoinPulseFlag = false;
  interrupts();

  isDispensing = true;
  dispenseStartedAt = millis();
  lastCoinDispensedAt = millis();

  Serial.println();
  Serial.print(F(">>> STARTING DISPENSE: Target = "));
  Serial.print(targetCoinCount);
  Serial.println(F(" Coins <<<"));
  Serial.println(F("Relay CLOSED -> Motor spinning..."));

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
  Serial.println(F("Tip: Manually pass a coin or opaque card through the sensor slot to verify it changes to LOW."));
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
    if (isDispensing) {
      Serial.print(F(" of "));
      Serial.print(targetCoinCount);
    }
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
      Serial.println(F("  Possible causes:"));
      Serial.println(F("   - Hopper is out of coins (empty)"));
      Serial.println(F("   - Coin jam in the spinning disc"));
      Serial.println(F("   - Sensor wiring disconnected (Pin D2)"));
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
    else if (input == "OFF") {
      Serial.println(F("MANUAL OVERRIDE: Turning Motor OFF."));
      isDispensing = false;
      turnMotorOff();
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
      Serial.println(F("'. Type HELP or enter a number (e.g. 10)."));
    }
  }
}

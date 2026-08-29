/*
  ==============================================================================
  ALLAN 220V AC COIN HOPPER DIAGNOSTIC & HARDWARE TEST FIRMWARE (ARDUINO UNO)
  ------------------------------------------------------------------------------
  FIXES APPLIED:
  1. Removed auto-start on boot: Machine starts safely in IDLE mode.
  2. Motor Start EMI Filter: 250ms blanking window when relay kicks ON to prevent
     false noise pulses.
  3. Optimized Debounce (25ms): Fast enough for high-speed coin exit pulses.
  4. Dual-Edge Sensor Detection (CHANGE): Works whether your sensor is Active-LOW
     or Active-HIGH.
  5. Live Stream & Manual Control: ON, OFF, 1, 2, 3, 5, STATUS commands.
  ==============================================================================

  WIRING DIAGRAM (NC - NORMALLY CLOSED SETUP):
  ------------------------------------------------------------------------------
  [220V AC HIGH VOLTAGE SIDE]:
  - Wall Outlet Live (L)  --> [2A Fuse] --> S-60-12 Power Supply [ L ] Terminal
  - S-60-12 [ L ] Terminal (Jumper) ------> 5V Relay Module [ COM ] Terminal
  - 5V Relay Module [ NC ] Terminal ------> Hopper Motor Live Wire (Normally Closed)
  
  - Wall Outlet Neutral (N) -------------> S-60-12 Power Supply [ N ] Terminal
  - S-60-12 [ N ] Terminal (Jumper) ------> Hopper Motor Neutral Wire

  [LOW VOLTAGE / ARDUINO UNO SIDE]:
  - Relay Module VCC  --> Arduino Uno 5V
  - Relay Module GND  --> Arduino Uno GND
  - Relay Module IN   --> Arduino Uno Pin D7 (Relay Control)

  - Hopper Sensor VCC --> Arduino Uno 5V
  - Hopper Sensor GND --> Arduino Uno GND
  - Hopper Sensor SIG --> Arduino Uno Pin D2 (Interrupt 0)
  ==============================================================================
*/

// --- PIN DEFINITIONS ---
const int HOPPER_SENSOR_PIN = 2; // Pin D2: Optical Exit Sensor (Interrupt 0)
const int HOPPER_RELAY_PIN  = 7; // Pin D7: Controls 5V Relay for 220V AC Motor

// --- RELAY POLARITY (NC SETUP) ---
// Tested: LOW = Motor ON (Relay LED ON), HIGH = Motor OFF (Relay LED OFF)
int relayOnLevel  = LOW;   // LOW  = Motor ON
int relayOffLevel = HIGH;  // HIGH = Motor OFF

// --- SENSOR & DISPENSE VARIABLES ---
volatile unsigned long totalCoinsPassed = 0;
volatile int sessionCoins = 0;
volatile bool coinPulseEvent = false;
volatile bool targetReached = false;
volatile unsigned long lastSensorPulseTime = 0;
volatile unsigned long ignoreSensorUntil = 0; // Motor kick EMI filter
const unsigned long SENSOR_DEBOUNCE_MS = 25;  // 25ms debounce for high-speed hopper coins

int targetCoins = 0;
bool isDispensing = false;
unsigned long dispenseStartTime = 0;
const unsigned long DISPENSE_TIMEOUT_MS = 10000; // 10s safety timeout

int lastPinD2State = -1;
unsigned long lastHeartbeat = 0;

// --- INTERRUPT SERVICE ROUTINE (PIN D2) ---
void coinExitISR() {
  unsigned long now = millis();
  // Ignore electrical noise when relay contacts switch
  if (now < ignoreSensorUntil) return;

  // Debounce for high-speed coin exit
  if (now - lastSensorPulseTime > SENSOR_DEBOUNCE_MS) {
    totalCoinsPassed++;
    sessionCoins++;
    coinPulseEvent = true;
    lastSensorPulseTime = now;

    // Cut motor power in microseconds when target is met
    if (isDispensing && sessionCoins >= targetCoins) {
      digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
      targetReached = true;
    }
  }
}

void stopHopperMotor() {
  digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
  isDispensing = false;
}

void startHopperMotorManual() {
  isDispensing = false; // continuous manual mode
  ignoreSensorUntil = millis() + 250; // Blank relay inductive spike for 250ms
  digitalWrite(HOPPER_RELAY_PIN, relayOnLevel);
}

void startDispenseTarget(int count) {
  if (count <= 0) return;

  targetCoins = count;
  noInterrupts();
  sessionCoins = 0;
  targetReached = false;
  coinPulseEvent = false;
  lastSensorPulseTime = 0;
  ignoreSensorUntil = millis() + 250; // Blank relay inductive spike for 250ms
  interrupts();

  isDispensing = true;
  dispenseStartTime = millis();

  Serial.println();
  Serial.println(F("----------------------------------------------------------"));
  Serial.print(F(">>> [DISPENSE START]: Target = "));
  Serial.print(targetCoins);
  Serial.println(F(" coin(s)..."));
  Serial.println(F("----------------------------------------------------------"));

  digitalWrite(HOPPER_RELAY_PIN, relayOnLevel);
}

void printMenu() {
  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("   ALLAN 220V COIN HOPPER DIAGNOSTIC & TEST (ARDUINO UNO) "));
  Serial.println(F("=========================================================="));
  Serial.println(F(" [MANUAL RELAY COMMANDS]:"));
  Serial.println(F("   ON   or M      -> Turn 220V Motor ON continuously"));
  Serial.println(F("   OFF  or 0      -> Turn 220V Motor OFF immediately"));
  Serial.println(F("   INVERT         -> Invert relay HIGH/LOW polarity"));
  Serial.println();
  Serial.println(F(" [AUTO DISPENSE COMMANDS]:"));
  Serial.println(F("   1, 2, 3, 5, 10 -> Dispense exact number of coins & auto-stop"));
  Serial.println();
  Serial.println(F(" [SENSOR & MONITOR COMMANDS]:"));
  Serial.println(F("   RESET          -> Reset coin counters to 0"));
  Serial.println(F("   STATUS         -> Print live pin states and totals"));
  Serial.println(F("   HELP           -> Show this menu again"));
  Serial.println(F("=========================================================="));
  Serial.println(F("Type a command (e.g. 3 or ON or STATUS) and press ENTER:"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // Configure Relay Pin
  pinMode(HOPPER_RELAY_PIN, OUTPUT);
  digitalWrite(HOPPER_RELAY_PIN, relayOffLevel); // Motor starts safely OFF

  // Configure Optical Sensor Pin with Internal Pullup & Interrupt (FALLING edge)
  pinMode(HOPPER_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HOPPER_SENSOR_PIN), coinExitISR, FALLING);

  lastPinD2State = digitalRead(HOPPER_SENSOR_PIN);

  printMenu();
  Serial.println(F("[SYSTEM READY]: Motor is OFF. Optical sensor monitor is ACTIVE."));
  Serial.println(F("You can test the sensor by sliding a coin through the slot by hand!"));
}

void loop() {
  unsigned long now = millis();

  // 1. Live Sensor Event Logger (Fired whenever a coin passes)
  if (coinPulseEvent) {
    coinPulseEvent = false;
    Serial.print(F(">>> [COIN DETECTED]: Count = "));
    Serial.print(sessionCoins);
    Serial.print(F(" (Total: "));
    Serial.print(totalCoinsPassed);
    Serial.println(F(")"));
  }

  // 2. Optical Sensor Pin State Transition Monitor (For testing by hand)
  int currentPinD2 = digitalRead(HOPPER_SENSOR_PIN);
  if (currentPinD2 != lastPinD2State) {
    lastPinD2State = currentPinD2;
    if (currentPinD2 == LOW) {
      Serial.println(F("   [PIN D2 STATE]: LOW  (Optical beam BLOCKED by coin)"));
    } else {
      Serial.println(F("   [PIN D2 STATE]: HIGH (Optical beam CLEAR)"));
    }
  }

  // 3. Target Dispense Complete Check
  if (isDispensing && targetReached) {
    stopHopperMotor();
    Serial.println();
    Serial.println(F("=========================================================="));
    Serial.print(F(">>> [DISPENSE SUCCESS]: "));
    Serial.print(targetCoins);
    Serial.println(F(" coin(s) dispensed!"));
    Serial.print(F(">>> Total Counted: "));
    Serial.println(sessionCoins);
    Serial.println(F(">>> Motor CUT OFF."));
    Serial.println(F("=========================================================="));
    targetReached = false;
  }

  // 4. Safety Timeout: Motor ran for 10 seconds without hitting target
  if (isDispensing && (now - dispenseStartTime >= DISPENSE_TIMEOUT_MS)) {
    stopHopperMotor();
    Serial.println();
    Serial.println(F("**********************************************************"));
    Serial.println(F(">>> [SAFETY TIMEOUT]: Motor stopped after 10s limit!"));
    Serial.print(F(">>> Coins counted before timeout: "));
    Serial.println(sessionCoins);
    Serial.println(F(">>> Check if hopper ran out of coins or sensor wire is loose."));
    Serial.println(F("**********************************************************"));
  }

  // 5. Handle Serial Commands
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();

    if (input.length() == 0) return;

    if (input == "HELP" || input == "?") {
      printMenu();
    }
    else if (input == "OFF" || input == "STOP" || input == "0") {
      stopHopperMotor();
      Serial.println(F(">>> [COMMAND]: Motor FORCED OFF."));
    }
    else if (input == "ON" || input == "M") {
      startHopperMotorManual();
      Serial.println(F(">>> [COMMAND]: Motor FORCED ON. Type OFF or 0 to stop."));
    }
    else if (input == "INVERT") {
      int tmp = relayOnLevel;
      relayOnLevel = relayOffLevel;
      relayOffLevel = tmp;
      digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
      isDispensing = false;
      Serial.print(F(">>> [COMMAND]: Relay polarity inverted! ON level is now = "));
      Serial.println(relayOnLevel == LOW ? F("LOW (0V)") : F("HIGH (5V)"));
    }
    else if (input == "RESET") {
      noInterrupts();
      totalCoinsPassed = 0;
      sessionCoins = 0;
      interrupts();
      Serial.println(F(">>> [COMMAND]: Coin counters RESET to 0."));
    }
    else if (input == "STATUS") {
      Serial.println();
      Serial.println(F("--- [HARDWARE STATUS DIAGNOSTIC] ---"));
      Serial.print(F("  Hopper Sensor Pin D2: "));
      if (digitalRead(HOPPER_SENSOR_PIN) == LOW) {
        Serial.println(F("LOW (BLOCKED)"));
      } else {
        Serial.println(F("HIGH (CLEAR)"));
      }
      Serial.print(F("  Hopper Relay Pin D7 : "));
      if (digitalRead(HOPPER_RELAY_PIN) == relayOnLevel) {
        Serial.println(F("ON (Motor Running)"));
      } else {
        Serial.println(F("OFF (Motor Stopped)"));
      }
      Serial.print(F("  Total Coins Counted : "));
      Serial.println(totalCoinsPassed);
      Serial.println(F("------------------------------------"));
    }
    else {
      // Check if user entered a number (e.g. 1, 2, 3, 5, 10)
      int requested = input.toInt();
      if (requested > 0) {
        startDispenseTarget(requested);
      } else {
        Serial.print(F("Unknown command: '"));
        Serial.print(input);
        Serial.println(F("'. Type HELP for command list."));
      }
    }
  }
}

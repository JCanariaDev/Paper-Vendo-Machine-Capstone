/*
  ==============================================================================
  ALLAN 220V AC COIN HOPPER HARDWARE TEST FIRMWARE (ARDUINO UNO)
  ------------------------------------------------------------------------------
  This sketch tests:
  1. Optical Exit Sensor (Counts coins leaving the hopper chute) on Pin D2
  2. 5V Relay Module (SRD-05VDC-SL-C) controlling the 220V AC Hopper Motor on Pin D7
  3. NC (Normally Closed) relay terminal configuration
  4. Automatic target dispensing (Default: 3 coins) with microsecond cutoff
  5. Jam / Empty hopper safety timeout (6 seconds max runtime)
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

// --- RELAY POLARITY FOR NC (NORMALLY CLOSED) SETUP ---
// With motor wired to NC:
// - LOW  (0V) = Relay Energized -> Pulls COM AWAY from NC -> Motor is OFF (Holding Brake)
// - HIGH (5V) = Relay De-energized -> COM connects to NC  -> Motor is ON  (Dispensing)
int relayOffLevel = LOW;   // LOW  = Motor OFF (Relay active, breaks NC connection)
int relayOnLevel  = HIGH;  // HIGH = Motor ON  (Relay inactive, completes NC connection)

// --- DISPENSE LOGIC VARIABLES ---
volatile int coinsDispensed = 0;
volatile bool targetReached = false;
volatile unsigned long lastSensorPulseTime = 0;
const unsigned long SENSOR_DEBOUNCE_MS = 120; // 120ms optical phototransistor debounce filter

int targetCoins = 3;            // Default test target: 3 coins
bool isDispensing = false;
unsigned long dispenseStartTime = 0;
const unsigned long DISPENSE_TIMEOUT_MS = 6000; // 6-second safety timeout (empty hopper / jam)

// --- INTERRUPT SERVICE ROUTINE (PIN D2) ---
void coinExitISR() {
  unsigned long now = millis();
  // Debounce to ensure a single coin passing the optical slit is counted exactly ONCE
  if (now - lastSensorPulseTime > SENSOR_DEBOUNCE_MS) {
    coinsDispensed++;
    lastSensorPulseTime = now;

    // The INSTANT target count is hit, cut power in microseconds inside the ISR
    if (isDispensing && coinsDispensed >= targetCoins) {
      digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
      targetReached = true;
    }
  }
}

// --- HELPER FUNCTIONS ---
void stopHopperMotor() {
  digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
  isDispensing = false;
}

void startDispense(int count) {
  if (count <= 0) return;

  targetCoins = count;
  noInterrupts();
  coinsDispensed = 0;
  targetReached = false;
  lastSensorPulseTime = 0;
  interrupts();

  isDispensing = true;
  dispenseStartTime = millis();

  Serial.println();
  Serial.print(F(">>> STARTING DISPENSE: Target = "));
  Serial.print(targetCoins);
  Serial.println(F(" coin(s)..."));

  // Power ON the 220V AC Motor
  digitalWrite(HOPPER_RELAY_PIN, relayOnLevel);
}

void printMenu() {
  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("     ARDUINO UNO - ALLAN 220V COIN HOPPER HARDWARE TEST   "));
  Serial.println(F("=========================================================="));
  Serial.println(F(" Commands to type in Serial Monitor:"));
  Serial.println(F("   1, 2, 3, 5, 10 -> Dispense that exact number of coins"));
  Serial.println(F("   ON             -> Turn motor ON continuously"));
  Serial.println(F("   OFF / STOP     -> Turn motor OFF immediately"));
  Serial.println(F("   INVERT         -> Flip relay HIGH/LOW active level"));
  Serial.println(F("   STATUS         -> Check pin states and total coin count"));
  Serial.println(F("   HELP           -> Show this menu again"));
  Serial.println(F("=========================================================="));
  Serial.println(F("Type a command (e.g. 3) and press ENTER:"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // Configure Relay Pin
  pinMode(HOPPER_RELAY_PIN, OUTPUT);
  digitalWrite(HOPPER_RELAY_PIN, relayOffLevel); // Ensure motor starts safely OFF

  // Configure Optical Sensor Pin with Internal Pullup & Interrupt
  pinMode(HOPPER_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HOPPER_SENSOR_PIN), coinExitISR, FALLING);

  printMenu();

  // Auto-start initial test of 3 coins after 2-second bootup
  Serial.println(F("[AUTO-TEST]: Automatically dispensing 3 coins in 2 seconds..."));
  delay(2000);
  startDispense(3);
}

void loop() {
  unsigned long now = millis();

  // 1. Check if ISR detected target completion
  if (isDispensing && targetReached) {
    stopHopperMotor();
    Serial.println();
    Serial.print(F("[SUCCESS]: Target of "));
    Serial.print(targetCoins);
    Serial.print(F(" coin(s) dispensed! Total counted: "));
    Serial.println(coinsDispensed);
    Serial.println(F("Motor relay CUT OFF."));
    targetReached = false;
  }

  // 2. Safety Timeout: Motor ran for 6 seconds without finishing target
  if (isDispensing && (now - dispenseStartTime >= DISPENSE_TIMEOUT_MS)) {
    stopHopperMotor();
    Serial.println();
    Serial.println(F("[SAFETY TIMEOUT]: Dispense stopped after 6s limit."));
    Serial.print(F("Coins dispensed before timeout: "));
    Serial.println(coinsDispensed);
    Serial.println(F("Check if hopper is empty or motor is jammed!"));
  }

  // 3. Handle Serial Monitor Commands
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
      Serial.println(F("Motor forced OFF."));
    }
    else if (input == "ON") {
      isDispensing = false; // continuous manual mode
      digitalWrite(HOPPER_RELAY_PIN, relayOnLevel);
      Serial.println(F("Motor forced ON continuously (Type STOP to turn off)."));
    }
    else if (input == "INVERT") {
      int tmp = relayOnLevel;
      relayOnLevel = relayOffLevel;
      relayOffLevel = tmp;
      digitalWrite(HOPPER_RELAY_PIN, relayOffLevel);
      isDispensing = false;
      Serial.print(F("Relay polarity flipped! ON level is now = "));
      Serial.println(relayOnLevel == LOW ? F("LOW (0V)") : F("HIGH (5V)"));
    }
    else if (input == "STATUS") {
      Serial.println(F("--- HARDWARE STATUS ---"));
      Serial.print(F("Optical Sensor (Pin D2): "));
      Serial.println(digitalRead(HOPPER_SENSOR_PIN) == LOW ? F("LOW (Coin Blocking / Active)") : F("HIGH (Clear)"));
      Serial.print(F("Relay Output (Pin D7): "));
      Serial.println(digitalRead(HOPPER_RELAY_PIN) == relayOnLevel ? F("ON (Motor Running)") : F("OFF (Motor Stopped)"));
      Serial.print(F("Total Coins Counted: "));
      Serial.println(coinsDispensed);
    }
    else {
      // Check if user entered a numeric target (e.g. 1, 3, 5, 10)
      int requestedCoins = input.toInt();
      if (requestedCoins > 0) {
        startDispense(requestedCoins);
      } else {
        Serial.print(F("Unknown command: '"));
        Serial.print(input);
        Serial.println(F("'. Type HELP for command list."));
      }
    }
  }
}

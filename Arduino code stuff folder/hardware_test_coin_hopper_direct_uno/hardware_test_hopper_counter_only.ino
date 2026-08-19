/*
  ==============================================================================
  ALLAN COIN HOPPER — DIRECT SENSOR & PULSE COUNTER TEST (NO RELAY)
  ------------------------------------------------------------------------------
  This sketch connects the Arduino Uno DIRECTLY to the Hopper's Mini PCB Sensor.
  - Arduino provides 5V & GND to power the optical sensor.
  - Arduino reads and counts coin pulses on Pin D2 in real-time.
  - AC power is plugged directly to the wall (motor spins continuously).
  ==============================================================================

  WIRING (Mini PCB to Arduino Uno):
  ------------------------------------------------------------------------------
  1. Red Wire (VCC)    --> Arduino Uno 5V
  2. Black Wire (GND)  --> Arduino Uno GND
  3. Signal Wire (SIG) --> Arduino Uno Pin D2 (Interrupt 0)

  POWER:
  - Arduino Uno: Plugged into PC via USB Cable
  - Hopper AC Cord: Plugged directly into 220V Wall Outlet
  ==============================================================================
*/

const int SENSOR_PIN = 2; // Pin connected to the Green PCB Signal Wire

volatile int totalCoins = 0;
volatile unsigned long lastPulseTime = 0;
volatile bool newCoinDetected = false;

const int TARGET_COINS = 3; // Set your target here (3 coins)

// Interrupt service routine triggered every time a coin breaks the IR beam
void onCoinPulse() {
  unsigned long now = millis();
  // 40ms debounce to prevent electrical double-counting
  if (now - lastPulseTime > 40) {
    totalCoins++;
    lastPulseTime = now;
    newCoinDetected = true;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), onCoinPulse, FALLING);

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("   ALLAN COIN HOPPER - DIRECT SENSOR COUNTER TEST (NO RELAY)"));
  Serial.println(F("=========================================================="));
  Serial.println(F(" Wiring:"));
  Serial.println(F("   Mini PCB Red (VCC)    --> Uno 5V"));
  Serial.println(F("   Mini PCB Black (GND)  --> Uno GND"));
  Serial.println(F("   Mini PCB Signal (SIG) --> Uno Pin D2"));
  Serial.println(F("----------------------------------------------------------"));
  Serial.print(F(" Target to reach: ")); Serial.print(TARGET_COINS); Serial.println(F(" Coins"));
  Serial.println(F(" Ready! Drop a coin or plug in AC power to start counting."));
  Serial.println(F(" (Type 'RESET' in Serial Monitor to reset counter to 0)"));
  Serial.println(F("=========================================================="));
  Serial.println();
}

void loop() {
  // 1. Print every coin pulse in real-time
  if (newCoinDetected) {
    newCoinDetected = false;

    Serial.print(F(" [COIN DETECTED #"));
    Serial.print(totalCoins);
    Serial.print(F(" / "));
    Serial.print(TARGET_COINS);
    Serial.println(F("]"));

    // Check if target reached
    if (totalCoins == TARGET_COINS) {
      Serial.println();
      Serial.println(F("**************************************************"));
      Serial.println(F("  TARGET REACHED! 3 COINS DISPENSED!              "));
      Serial.println(F("  --> UNPLUG 220V AC POWER NOW TO STOP MOTOR! <-- "));
      Serial.println(F("**************************************************"));
      Serial.println();
    }
  }

  // 2. Handle serial commands (e.g. RESET)
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "RESET" || cmd == "0") {
      noInterrupts();
      totalCoins = 0;
      newCoinDetected = false;
      interrupts();
      Serial.println(F(">> Coin counter reset to 0. Ready for next test! <<"));
    }
    else if (cmd == "STATUS" || cmd == "READ") {
      int pinState = digitalRead(SENSOR_PIN);
      Serial.print(F("Live Sensor Pin D2: "));
      Serial.println(pinState == HIGH ? F("HIGH (Beam clear)") : F("LOW (Beam blocked/Coin present)"));
      Serial.print(F("Current Coin Count: "));
      Serial.println(totalCoins);
    }
  }
}

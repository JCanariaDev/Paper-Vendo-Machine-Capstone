/*
  ==============================================================================
  CALIBRATED COIN HOPPER SENSOR TEST (THRESHOLD = 3.3V)
  ------------------------------------------------------------------------------
  EXACT HARDWARE CALIBRATION:
  - Beam CLEAR   = 4.10V (Voltage > 3.3V) -> Slit is open
  - Beam BLOCKED = 2.59V (Voltage < 3.3V) -> Coin detected! (Count + 1)
  ==============================================================================

  WIRING (3 WIRES ONLY):
  - Hopper Sensor VCC (+5V) --> Arduino Uno 5V Pin
  - Hopper Sensor GND (GND) --> Arduino Uno GND Pin
  - Hopper Sensor SIG (OUT) --> Arduino Uno Pin A0
  ==============================================================================
*/

const int SENSOR_PIN = A0;   // Analog Pin A0
const int ONBOARD_LED = 13;  // Uno Pin 13 LED (lights up on detection)

const float THRESHOLD_VOLTS = 3.30; // Switching midpoint between 4.10V and 2.59V
const unsigned long DEBOUNCE_MS = 35; // 35ms debounce

unsigned long coinCount = 0;
bool isCurrentlyBlocked = false;
unsigned long lastChangeTime = 0;
unsigned long lastStreamTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, LOW);
  pinMode(SENSOR_PIN, INPUT);

  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("   CALIBRATED COIN HOPPER COUNTER (THRESHOLD: 3.3V)       "));
  Serial.println(F("=========================================================="));
  Serial.println(F(" CALIBRATED VALUES:"));
  Serial.println(F("   CLEAR   = 4.10V (Idle)"));
  Serial.println(F("   BLOCKED = 2.59V (Coin Detected!)"));
  Serial.println(F("----------------------------------------------------------"));
  Serial.println(F(" Slide cardboard / drop coins through the sensor now:"));
  Serial.println(F("=========================================================="));
  Serial.println();
}

void loop() {
  int aRaw = analogRead(SENSOR_PIN);
  float voltage = aRaw * (5.0 / 1023.0);
  unsigned long now = millis();

  // Check if voltage is below threshold (Blocked)
  bool blockedState = (voltage < THRESHOLD_VOLTS);

  if (blockedState != isCurrentlyBlocked) {
    if (now - lastChangeTime > DEBOUNCE_MS) {
      isCurrentlyBlocked = blockedState;
      lastChangeTime = now;

      if (isCurrentlyBlocked) {
        // Coin entered the slit!
        coinCount++;
        digitalWrite(ONBOARD_LED, HIGH);

        Serial.println();
        Serial.println(F("**************************************************"));
        Serial.print(F(">>> [COIN DETECTED!]: Count = "));
        Serial.print(coinCount);
        Serial.print(F(" | Voltage = "));
        Serial.print(voltage, 2);
        Serial.println(F(" V"));
        Serial.println(F("**************************************************"));
        Serial.println();
      } else {
        // Coin left the slit
        digitalWrite(ONBOARD_LED, LOW);
        Serial.print(F(">>> [BEAM CLEARED]: Voltage = "));
        Serial.print(voltage, 2);
        Serial.print(F(" V | Total Coins = "));
        Serial.println(coinCount);
      }
    }
  }

  // Live status stream every 1 second
  if (now - lastStreamTime >= 1000) {
    lastStreamTime = now;
    Serial.print(F("[STATUS] Live Voltage: "));
    Serial.print(voltage, 2);
    Serial.print(F(" V | State: "));
    Serial.print(isCurrentlyBlocked ? F("BLOCKED (Coin in slit)") : F("CLEAR (Idle)"));
    Serial.print(F(" | Total Coins Counted: "));
    Serial.println(coinCount);
  }

  // Handle Reset command
  if (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "RESET" || cmd == "0") {
      coinCount = 0;
      Serial.println(F(">>> [RESET]: Coin counter reset to 0."));
    }
  }
}

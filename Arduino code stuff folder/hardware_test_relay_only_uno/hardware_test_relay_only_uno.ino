/*
  ==============================================================================
  PURE RELAY ON/OFF HARDWARE TEST (ARDUINO UNO)
  ------------------------------------------------------------------------------
  This sketch tests ONLY the 5V Relay Module (SRD-05VDC-SL-C) and 220V Hopper Motor.
  No sensor logic, no coin counting — just pure electrical relay switching control.
  ==============================================================================

  WIRING:
  - Relay Module VCC --> Arduino Uno 5V
  - Relay Module GND --> Arduino Uno GND
  - Relay Module IN  --> Arduino Uno Pin D7
  - 220V AC Live     --> [2A Fuse] --> Relay COM
  - Relay NO         --> Hopper Motor Live
  - 220V AC Neutral  --> Hopper Motor Neutral directly
  ==============================================================================
*/

const int RELAY_PIN = 7; // Relay IN pin connected to Arduino D7

bool isBlinking = false;
unsigned long lastBlinkTime = 0;
bool currentBlinkState = false;

void printMenu() {
  Serial.println();
  Serial.println(F("=========================================================="));
  Serial.println(F("       ARDUINO UNO - PURE RELAY ON/OFF TEST FIRMWARE      "));
  Serial.println(F("=========================================================="));
  Serial.println(F(" Commands to type in Serial Monitor:"));
  Serial.println(F("   1   or HIGH   -> Output HIGH (5V) to Pin D7"));
  Serial.println(F("   0   or LOW    -> Output LOW  (0V) to Pin D7"));
  Serial.println(F("   2S            -> Turn ON for 2 seconds, then auto-OFF"));
  Serial.println(F("   BLINK         -> Toggle ON/OFF every 1.5 seconds"));
  Serial.println(F("   STOP          -> Stop blinking / turn everything OFF"));
  Serial.println(F("   HELP          -> Show this menu"));
  Serial.println(F("=========================================================="));
  Serial.println(F("Type a command (e.g. 1 or 0 or 2S) and press ENTER:"));
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Start at LOW

  printMenu();
}

void loop() {
  // Handle continuous blinking test if active
  if (isBlinking) {
    if (millis() - lastBlinkTime >= 1500) {
      lastBlinkTime = millis();
      currentBlinkState = !currentBlinkState;
      digitalWrite(RELAY_PIN, currentBlinkState ? HIGH : LOW);

      Serial.print(F("[BLINKING] Pin D7 set to: "));
      Serial.println(currentBlinkState ? F("HIGH (5V)") : F("LOW (0V)"));
      Serial.println(F("--> Listen for relay physical CLICK and observe motor state!"));
    }
  }

  // Handle Serial Commands
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd.length() == 0) return;

    if (cmd == "1" || cmd == "HIGH") {
      isBlinking = false;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println();
      Serial.println(F("[RELAY] --> Outputting HIGH (5V) on Pin D7"));
      Serial.println(F("Did the red/green LED on the relay board turn ON or OFF?"));
      Serial.println(F("Did the 220V motor START or STOP?"));
      Serial.println();
    }
    else if (cmd == "0" || cmd == "LOW") {
      isBlinking = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println();
      Serial.println(F("[RELAY] --> Outputting LOW (0V / GND) on Pin D7"));
      Serial.println(F("Did the red/green LED on the relay board turn ON or OFF?"));
      Serial.println(F("Did the 220V motor START or STOP?"));
      Serial.println();
    }
    else if (cmd == "2S" || cmd == "PULSE" || cmd == "TEST") {
      isBlinking = false;
      Serial.println();
      Serial.println(F("[2-SECOND TEST]"));
      
      // Test State A
      Serial.println(F("Step 1: Setting Pin D7 to HIGH for 2 seconds..."));
      digitalWrite(RELAY_PIN, HIGH);
      delay(2000);

      // Test State B
      Serial.println(F("Step 2: Setting Pin D7 to LOW for 2 seconds..."));
      digitalWrite(RELAY_PIN, LOW);
      delay(2000);

      Serial.println(F("Step 3: Done! Observe which step made the motor spin."));
      Serial.println();
    }
    else if (cmd == "BLINK") {
      isBlinking = true;
      lastBlinkTime = millis();
      currentBlinkState = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println(F("[BLINK MODE ACTIVATED] Toggling Pin D7 every 1.5s. Type 'STOP' to end."));
    }
    else if (cmd == "STOP" || cmd == "OFF") {
      isBlinking = false;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println(F("[STOPPED] Pin D7 set to LOW."));
    }
    else if (cmd == "HELP" || cmd == "?") {
      printMenu();
    }
    else {
      Serial.print(F("Unknown command: '")); Serial.print(cmd); Serial.println(F("'. Type 1, 0, 2S, or BLINK."));
    }
  }
}

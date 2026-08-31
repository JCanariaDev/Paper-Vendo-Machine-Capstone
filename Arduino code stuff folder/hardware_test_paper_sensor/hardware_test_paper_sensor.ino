/*
  ==============================================================================
  HARDWARE TEST — SINGLE PAPER SENSOR (L5290 / IR MODULE)
  Works on: Arduino MEGA 2560 and Arduino UNO
  ==============================================================================

  WIRING (Looking at the sensor with 3 pins facing DOWN):
  ------------------------------------------------------------------------------
    - Pin 1 (Left Pin)   --> Arduino 5V
    - Pin 2 (Middle Pin) --> Arduino GND
    - Pin 3 (Right Pin)  --> Arduino Digital Pin 2 (Signal)

  ONBOARD LED:
  ------------------------------------------------------------------------------
    - Arduino Pin 13 LED will light up automatically whenever the sensor is
      triggered/blocked!

  SERIAL MONITOR:
  ------------------------------------------------------------------------------
    - Baud Rate: 9600
    - Real-time instant status printout whenever you insert or remove paper.
  ==============================================================================
*/

const int SENSOR_PIN = 2;        // Connect Sensor Pin 3 (OUT/SIG) here
const int LED_PIN    = 13;       // Onboard LED for instant visual feedback

int lastState = -1;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println(F("=================================================="));
  Serial.println(F("    SINGLE PAPER SENSOR TEST — READY"));
  Serial.println(F("=================================================="));
  Serial.println(F("Wiring check:"));
  Serial.println(F("  - Sensor Pin 1 (Left)   --> 5V"));
  Serial.println(F("  - Sensor Pin 2 (Middle) --> GND"));
  Serial.println(F("  - Sensor Pin 3 (Right)  --> Pin 2"));
  Serial.println(F("--------------------------------------------------"));
  Serial.println(F("Insert a card / paper into the sensor slot..."));
  Serial.println(F("--------------------------------------------------"));

  // Initial read
  lastState = digitalRead(SENSOR_PIN);
  printCurrentState(lastState);
}

void printCurrentState(int state) {
  if (state == HIGH) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println(F("[STATE: HIGH] --> BLOCKED (Paper / Card Detected!) | LED: ON"));
  } else {
    digitalWrite(LED_PIN, LOW);
    Serial.println(F("[STATE: LOW ] --> OPEN / CLEAR (No Paper)         | LED: OFF"));
  }
}

void loop() {
  int currentState = digitalRead(SENSOR_PIN);

  // Print only when the state changes
  if (currentState != lastState) {
    lastState = currentState;
    printCurrentState(currentState);
  }

  delay(50); // Small debounce delay
}


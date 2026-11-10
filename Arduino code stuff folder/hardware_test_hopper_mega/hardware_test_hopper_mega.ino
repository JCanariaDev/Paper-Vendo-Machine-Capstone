/*
  Standalone coin-hopper test for an extra Arduino Mega 2560.

  Matches the main Mega hopper pins and active-low relay-module logic:
    D22 -> relay module IN (LOW energizes relay / requests motor ON)
    D23 <- hopper exit sensor (LOW while beam is blocked)
  D23 uses an external 10k/20k signal divider; the 20k resistor must pull
  the D23 junction down to the same Mega GND.
    GND -> shared low-voltage ground for relay module and sensor

  IMPORTANT: This sketch assumes the hopper motor circuit uses relay COM + NO,
  so the motor is OFF while the relay is de-energized. Do not use COM + NC for
  this test: NC is closed while the relay is idle and can make the hopper run
  at boot or reset.

  Serial Monitor: 115200 baud, Newline
    RELEASE 3  - run hopper until 3 sensor pulses are counted
    RELAY ON   - energize relay manually (15-second safety limit)
    RELAY OFF  - de-energize relay
    STOP       - stop a release immediately
    STATUS     - show relay, sensor, and release state
    MONITOR ON/OFF - print sensor state changes while idle or releasing
*/

const uint8_t HOPPER_RELAY_PIN = 22;
const uint8_t HOPPER_SENSOR_PIN = 23;

// Matches the main Mega's active-low relay module.
const uint8_t RELAY_ON_LEVEL = LOW;
const uint8_t RELAY_OFF_LEVEL = HIGH;
const uint8_t SENSOR_BLOCKED_LEVEL = LOW;

const unsigned long SENSOR_DEBOUNCE_MS = 25;
const unsigned long RELEASE_TIMEOUT_MS = 15000;
const unsigned long MANUAL_RELAY_TIMEOUT_MS = 15000;
const uint8_t MAX_RELEASE_COINS = 20;

bool releaseRunning = false;
bool manualRelayOn = false;
uint8_t targetCoins = 0;
uint8_t countedCoins = 0;
bool sensorRawBlocked = false;
bool sensorStableBlocked = false;
bool sensorMonitorEnabled = true;
unsigned long sensorCandidateSince = 0;
unsigned long releaseStartedAt = 0;
unsigned long manualRelayStartedAt = 0;
unsigned long lastAcceptedPulseAt = 0;
String inputLine;

void stopRelease(const __FlashStringHelper *reason) {
  digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF_LEVEL);
  manualRelayOn = false;
  if (releaseRunning) {
    releaseRunning = false;
    Serial.print(F("HOPPER STOPPED: "));
    Serial.println(reason);
    Serial.print(F("HOPPER RESULT: "));
    Serial.print(countedCoins);
    Serial.print('/');
    Serial.println(targetCoins);
  }
}

void beginRelease(uint8_t requestedCoins) {
  if (releaseRunning) {
    Serial.println(F("BUSY: send STOP before another RELEASE."));
    return;
  }
  if (manualRelayOn) {
    Serial.println(F("BUSY: send RELAY OFF before a counted RELEASE."));
    return;
  }

  if (requestedCoins < 1 || requestedCoins > MAX_RELEASE_COINS) {
    Serial.print(F("Invalid count. Use RELEASE 1 through RELEASE "));
    Serial.println(MAX_RELEASE_COINS);
    return;
  }

  if (sensorStableBlocked) {
    Serial.println(F("NOT STARTED: sensor is blocked. Clear the exit sensor first."));
    return;
  }

  targetCoins = requestedCoins;
  countedCoins = 0;
  releaseStartedAt = millis();
  lastAcceptedPulseAt = releaseStartedAt - SENSOR_DEBOUNCE_MS;
  releaseRunning = true;

  Serial.print(F("RELEASE STARTED: target="));
  Serial.println(targetCoins);
  digitalWrite(HOPPER_RELAY_PIN, RELAY_ON_LEVEL);
}

void printStatus() {
  Serial.println(F("--- HOPPER TEST STATUS ---"));
  Serial.print(F("Relay D22: "));
  Serial.println(digitalRead(HOPPER_RELAY_PIN) == RELAY_ON_LEVEL ? F("ON") : F("OFF"));
  Serial.print(F("Sensor D23: "));
  Serial.println(sensorStableBlocked
                     ? F("LOW / BLOCKED") : F("HIGH / CLEAR"));
  Serial.print(F("Release: "));
  if (releaseRunning) {
    Serial.print(F("RUNNING "));
    Serial.print(countedCoins);
    Serial.print('/');
    Serial.println(targetCoins);
  } else {
    Serial.println(F("IDLE"));
  }
  Serial.print(F("Manual relay: "));
  Serial.println(manualRelayOn ? F("ON (safety timer active)") : F("OFF"));
}

void handleCommand(String command) {
  command.trim();
  command.toUpperCase();
  if (!command.length()) return;

  if (command == "STOP") {
    const bool wasRunning = releaseRunning;
    const bool wasManual = manualRelayOn;
    stopRelease(F("manual STOP command"));
    if (!wasRunning && wasManual) {
      Serial.println(F("MANUAL RELAY OFF: manual STOP command"));
    } else if (!wasRunning && !wasManual) {
      Serial.println(F("Already stopped; relay remains OFF."));
    }
    return;
  }

  if (command == "RELAY ON") {
    if (releaseRunning) {
      Serial.println(F("BUSY: send STOP before manual relay control."));
      return;
    }
    if (manualRelayOn) {
      Serial.println(F("Manual relay is already ON; send RELAY OFF to stop."));
      return;
    }
    manualRelayOn = true;
    manualRelayStartedAt = millis();
    digitalWrite(HOPPER_RELAY_PIN, RELAY_ON_LEVEL);
    Serial.println(F("MANUAL RELAY ON: sensor pulses are not counted; auto-off in 15 seconds."));
    return;
  }

  if (command == "RELAY OFF") {
    digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF_LEVEL);
    manualRelayOn = false;
    Serial.println(F("MANUAL RELAY OFF"));
    return;
  }

  if (command == "STATUS") {
    printStatus();
    return;
  }

  if (command == "MONITOR ON") {
    sensorMonitorEnabled = true;
    Serial.println(F("Sensor change monitor ON; reports BLOCKED/CLEARED transitions only."));
    printStatus();
    return;
  }

  if (command == "MONITOR OFF") {
    sensorMonitorEnabled = false;
    Serial.println(F("Sensor change monitor OFF; release pulse counting remains active."));
    return;
  }

  if (command.startsWith("RELEASE ")) {
    String value = command.substring(8);
    value.trim();
    if (!value.length()) {
      Serial.println(F("Usage: RELEASE <coin_count>"));
      return;
    }
    for (unsigned int i = 0; i < value.length(); i++) {
      if (!isDigit(value[i])) {
        Serial.println(F("Invalid count. Example: RELEASE 3"));
        return;
      }
    }
    const long requestedCoins = value.toInt();
    if (requestedCoins < 1 || requestedCoins > MAX_RELEASE_COINS) {
      Serial.print(F("Invalid count. Use RELEASE 1 through RELEASE "));
      Serial.println(MAX_RELEASE_COINS);
      return;
    }
    beginRelease((uint8_t)requestedCoins);
    return;
  }

  Serial.println(F("Commands: RELEASE <1-20>, RELAY ON, RELAY OFF, STOP, STATUS, MONITOR ON, MONITOR OFF"));
}

void serviceSerial() {
  while (Serial.available()) {
    const char c = (char)Serial.read();
    if (c == '\n') {
      handleCommand(inputLine);
      inputLine = "";
    } else if (c != '\r') {
      if (inputLine.length() < 48) inputLine += c;
      else inputLine = "";
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Preload the idle level before making the relay pin an output.
  digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF_LEVEL);
  pinMode(HOPPER_RELAY_PIN, OUTPUT);
  // Match the main Mega sketch. The internal pull-up may affect the divider's
  // measured voltage, so retain the external divider and shared ground.
  pinMode(HOPPER_SENSOR_PIN, INPUT_PULLUP);
  sensorRawBlocked = (digitalRead(HOPPER_SENSOR_PIN) == SENSOR_BLOCKED_LEVEL);
  sensorStableBlocked = sensorRawBlocked;
  sensorCandidateSince = millis();

  Serial.println(F("MEGA COIN HOPPER TEST READY"));
  Serial.println(F("Confirm motor is wired through relay COM + NO, not COM + NC."));
  Serial.println(F("Serial Monitor: 115200 baud, Newline. Commands: RELEASE 3, RELAY ON/OFF, STOP, STATUS, MONITOR ON/OFF"));
  printStatus();
}

void loop() {
  serviceSerial();

  const unsigned long now = millis();
  const bool rawBlocked = (digitalRead(HOPPER_SENSOR_PIN) == SENSOR_BLOCKED_LEVEL);

  if (rawBlocked != sensorRawBlocked) {
    sensorRawBlocked = rawBlocked;
    sensorCandidateSince = now;
  }

  // Accept a state change only after it remains stable through the debounce window.
  if (sensorRawBlocked != sensorStableBlocked &&
      now - sensorCandidateSince >= SENSOR_DEBOUNCE_MS) {
    sensorStableBlocked = sensorRawBlocked;

    if (sensorMonitorEnabled) {
      Serial.println(sensorStableBlocked ? F("SENSOR BLOCKED") : F("SENSOR CLEARED"));
    }

    // Count one coin on each stable clear-to-blocked transition while dispensing.
    if (sensorStableBlocked && releaseRunning &&
        now - lastAcceptedPulseAt >= SENSOR_DEBOUNCE_MS) {
      lastAcceptedPulseAt = now;
      countedCoins++;
      Serial.print(F("HOPPER DETECTED: "));
      Serial.print(countedCoins);
      Serial.print('/');
      Serial.println(targetCoins);

      if (countedCoins >= targetCoins) {
        stopRelease(F("requested count reached"));
      }
    }
  }

  if (releaseRunning && now - releaseStartedAt >= RELEASE_TIMEOUT_MS) {
    stopRelease(F("15-second safety timeout"));
  }

  if (manualRelayOn && now - manualRelayStartedAt >= MANUAL_RELAY_TIMEOUT_MS) {
    digitalWrite(HOPPER_RELAY_PIN, RELAY_OFF_LEVEL);
    manualRelayOn = false;
    Serial.println(F("MANUAL RELAY AUTO-OFF: 15-second safety timeout"));
  }
}

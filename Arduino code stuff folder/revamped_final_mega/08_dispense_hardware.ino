// DISPENSE HARDWARE
// Split from revamped_final_mega.ino for readability.

bool dispenseOnePen(int channel) {
  int penIndex = channel - 1;
  if (penIndex < 0 || penIndex >= BALLPEN_COUNT) return false;
  Stepper* pen = penSteppers[penIndex];
  int irPin = penIrPins[penIndex];
  if (digitalRead(irPin) == LOW) {
    Serial.println("PEN ABORT: IR is LOW before dispense; chute blocked.");
    return false;
  }
  pen->step(1024);
  unsigned long startedAt = millis();
  bool detected = false;
  while (millis() - startedAt < PEN_SENSOR_TIMEOUT_MS) {
    if (digitalRead(irPin) == LOW) { detected = true; break; }
  }
  delay(300);
  pen->step(-1024);
  stopStepper(penIndex);
  return detected;
}

void stopStepper(int penIndex) {
  for (int p = 0; p < 4; p++) digitalWrite(penStopPins[penIndex][p], LOW);
}

int releaseVerifiedChange(int changeCents) {
  if (changeCents <= 0) return 0;
  if (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW) {
    Serial.println("HOPPER WARNING: exit sensor is LOW at start.");
  }
  const int expectedCoins = changeCents / 100;
  int countedCoins = 0;
  bool previousBlocked = false;
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
  unsigned long lastCoinAt = millis();
  while (countedCoins < expectedCoins && millis() - lastCoinAt < CHANGE_COIN_TIMEOUT_MS) {
    bool blocked = (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW);
    if (blocked && !previousBlocked) {
      countedCoins++;
      lastCoinAt = millis();
    }
    previousBlocked = blocked;
  }
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  return countedCoins * 100; // Returns exact amount released
}


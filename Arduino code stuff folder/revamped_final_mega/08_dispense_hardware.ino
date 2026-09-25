// DISPENSE HARDWARE
// Paper is delegated to Paper Uno; ballpens are delegated to Ballpen Uno.

const unsigned long BALLPEN_DISPENSE_TIMEOUT_PER_ITEM_MS = 8000;
const unsigned long BALLPEN_DISPENSE_TIMEOUT_MARGIN_MS = 2500;

void handleBallpenMessage(String msg) {
  msg.trim();
  if (msg.length() == 0) return;
  if (msg == "BALLPEN_READY") {
    ballpenUnoResponsive = true;
    return;
  }
  Serial.print("Ballpen Uno: ");
  Serial.println(msg);
}

int dispensePenFromUno(int channel, int quantity) {
  if (channel < 1 || channel > BALLPEN_COUNT || quantity <= 0) return 0;

  // Format: DISPENSE:<channel>:<quantity>
  BALLPEN_SERIAL.println("DISPENSE:" + String(channel) + ":" + String(quantity));

  const unsigned long timeoutMs =
    BALLPEN_DISPENSE_TIMEOUT_MARGIN_MS +
    (BALLPEN_DISPENSE_TIMEOUT_PER_ITEM_MS * (unsigned long)quantity);
  const unsigned long startedAt = millis();
  while (millis() - startedAt < timeoutMs) {
    if (!BALLPEN_SERIAL.available()) {
      delay(2);
      continue;
    }

    String response = BALLPEN_SERIAL.readStringUntil('\n');
    response.trim();

    if (response.startsWith("BALLPEN_DONE:")) {
      // Format: BALLPEN_DONE:<channel>:<count>
      int first = response.indexOf(':');
      int second = response.indexOf(':', first + 1);
      if (first < 0 || second < 0) return 0;
      return response.substring(second + 1).toInt();
    }

    if (response.startsWith("BALLPEN_FAIL:")) {
      Serial.print("Ballpen dispense failed: ");
      Serial.println(response);
      // Format: BALLPEN_FAIL:<channel>:<count>:<reason>
      int first = response.indexOf(':');
      int second = response.indexOf(':', first + 1);
      int third = response.indexOf(':', second + 1);
      if (first < 0 || second < 0) return 0;
      return third < 0 ? response.substring(second + 1).toInt()
                       : response.substring(second + 1, third).toInt();
    }

    handleBallpenMessage(response);
  }

  BALLPEN_SERIAL.println("STOP");
  Serial.println("Ballpen Uno dispense timeout; STOP sent.");
  return 0;
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
  return countedCoins * 100;
}

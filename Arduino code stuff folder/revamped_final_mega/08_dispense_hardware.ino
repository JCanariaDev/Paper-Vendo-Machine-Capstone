// DISPENSE HARDWARE
// Paper is delegated to Paper Uno; ballpens are delegated to Ballpen Uno.

const unsigned long BALLPEN_DISPENSE_TIMEOUT_PER_ITEM_MS = 12000;
const unsigned long BALLPEN_DISPENSE_TIMEOUT_MARGIN_MS = 3000;

void handleBallpenMessage(String msg) {
  msg.trim();
  if (msg.length() == 0) return;
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
      int count = response.substring(second + 1).toInt();
      if (count > 0) {
        BALLPEN_SERIAL.println("BEEP:1800:150");
      }
      return count;
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
  bool previousBlocked = (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW);
  Serial.print("HOPPER: starting sensor state = ");
  Serial.println(previousBlocked ? "BLOCKED (LOW)" : "CLEAR (HIGH)");
  if (previousBlocked) {
    Serial.println("HOPPER WARNING: exit sensor is LOW at start.");
    Serial.println("HOPPER: waiting for a clear-to-blocked transition; a coin already blocking the beam will not count yet.");
  }
  const int expectedCoins = changeCents / 100;
  int countedCoins = 0;
  bool coinInBeam = false;
  Serial.print("HOPPER: requested change=P");
  Serial.print(changeCents / 100.0, 2);
  Serial.print("; expected coin pulses=");
  Serial.println(expectedCoins);
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_ON);
  Serial.println("HOPPER: relay ON; monitoring D23 (LOW=beam blocked).");
  unsigned long lastCoinAt = millis();
  while (countedCoins < expectedCoins && millis() - lastCoinAt < CHANGE_COIN_TIMEOUT_MS) {
    bool blocked = (digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW);
    if (blocked && !previousBlocked) {
      coinInBeam = true;
      lastCoinAt = millis();
      Serial.println("HOPPER SENSOR: beam blocked; coin entered sensor.");
    } else if (!blocked && previousBlocked) {
      if (coinInBeam) {
        countedCoins++;
        coinInBeam = false;
        lastCoinAt = millis();
        Serial.print("HOPPER SENSOR: beam cleared; coin passage confirmed ");
        Serial.print(countedCoins);
        Serial.print("/");
        Serial.println(expectedCoins);
        BALLPEN_SERIAL.println("BEEP:2000:70"); // Coin passage confirmed
      } else {
        Serial.println("HOPPER SENSOR: beam clear; ready for coin.");
      }
    }
    previousBlocked = blocked;
  }
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  Serial.println("HOPPER: relay OFF.");
  if (countedCoins < expectedCoins) {
    Serial.print("HOPPER TIMEOUT: confirmed ");
    Serial.print(countedCoins);
    Serial.print("/");
    Serial.print(expectedCoins);
    Serial.println(" coins before timeout.");
  } else {
    Serial.print("HOPPER COMPLETE: sensor confirmed ");
    Serial.print(countedCoins);
    Serial.println(" coin(s).");
  }
  return countedCoins * 100;
}

// INDICATORS COIN
// Split from revamped_final_mega.ino for readability.

void setMachineIndicator(IndicatorState state, bool sound) {
  indicatorState = state;
  digitalWrite(LED_GREEN_PIN, state == INDICATOR_READY  ? HIGH : LOW);
  digitalWrite(LED_BLUE_PIN,  state == INDICATOR_ACTIVE ? HIGH : LOW);
  digitalWrite(LED_RED_PIN,   state == INDICATOR_ERROR  ? HIGH : LOW);

  if (!sound) return;
  switch (state) {
    case INDICATOR_READY:
      tone(BUZZER_PIN, 1800, 80);
      delay(140);
      tone(BUZZER_PIN, 1800, 80);
      break;
    case INDICATOR_ACTIVE:
      tone(BUZZER_PIN, 1100, 120);
      break;
    case INDICATOR_ERROR:
      tone(BUZZER_PIN, 350, 500);
      break;
  }
}

void refreshMachineAvailability(bool sound) {
  if (orderInProgress || wifiStatus == WIFI_STATUS_CONNECTING || wifiStatus == WIFI_STATUS_IDLE) {
    setMachineIndicator(INDICATOR_ACTIVE, sound);
  } else if (wifiStatus == WIFI_STATUS_CONNECTED) {
    setMachineIndicator(INDICATOR_READY, sound);
  } else {
    setMachineIndicator(INDICATOR_ERROR, sound);
  }
}

void setCoinAcceptance(bool allowed) {
  if (credits >= MAX_CREDITS_ALLOWED) {
    allowed = false;
  }
  coinAcceptorEnabled = allowed;
  ignoreCoinPulsesUntil = millis() + 600;  
  int targetLevel = allowed ? coinRelayOnLevel : coinRelayOffLevel;
  digitalWrite(COIN_INHIBIT_PIN, targetLevel);
}

void coinInterrupt() {
  if (orderInProgress) return;
  // Hard software gate: If acceptor was cut off and not waiting for burst remainder, reject pulse!
  if (!coinAcceptorEnabled && !pendingCoinAcceptorOff) return;

  unsigned long now = millis();
  // Anti-glitch: Ignore power surge / relay transient noise on Pin D2
  // (only active after MANUAL relay switching, not after coin-triggered cutoff)
  if (now < ignoreCoinPulsesUntil) return;

  static unsigned long lastPulse = 0;
  // 50ms debounce: filters electrical noise while still capturing all pulse bursts
  // from ?1 (1 pulse), ?5 (5 pulses), ?10 (10 pulses), ?20 (20 pulses)
  // Coin acceptors typically send pulses 50-80ms apart within a burst.
  if (now - lastPulse > 50) {
    credits++;            // Count every pulse — including the remainder of a multi-peso coin
    coinPulseReceived = true;
    lastCoinBurstTime = now;  // Track when the last pulse arrived
    lastPulse = now;

    if (credits >= MAX_CREDITS_ALLOWED) {
      // Option A: Do NOT cut relay here.
      // Queue the cutoff and let loop() fire it only after 350ms of silence,
      // so all remaining pulses of the current coin are fully counted first.
      pendingCoinAcceptorOff = true;
    }
  }
}


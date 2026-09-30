// INDICATORS COIN
// Split from revamped_final_mega.ino for readability.

void setMachineIndicator(IndicatorState state, bool sound) {
  indicatorState = state;
  const char* stateName = state == INDICATOR_READY ? "READY" :
                          state == INDICATOR_ACTIVE ? "ACTIVE" : "ERROR";
  BALLPEN_SERIAL.println(String("INDICATOR:") + stateName);

  if (!sound) return;
  switch (state) {
    case INDICATOR_READY:
      BALLPEN_SERIAL.println("BEEP:1800:80");
      delay(140);
      BALLPEN_SERIAL.println("BEEP:1800:80");
      break;
    case INDICATOR_ACTIVE:
      BALLPEN_SERIAL.println("BEEP:1100:120");
      break;
    case INDICATOR_ERROR:
      BALLPEN_SERIAL.println("BEEP:350:500");
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
  if (credits >= maximumCreditsAllowed) {
    allowed = false;
  }
  int targetLevel = allowed ? coinRelayOnLevel : coinRelayOffLevel;
  coinAcceptorEnabled = allowed;
  bool relayChanged = digitalRead(COIN_INHIBIT_PIN) != targetLevel;
  digitalWrite(COIN_INHIBIT_PIN, targetLevel);
  // Do not suppress the first real coin after enabling the acceptor. The old
  // settle delay caused the first pulse of a multi-pulse coin to be dropped,
  // making a 5-peso coin look like 1 peso on the first insertion. Relay noise
  // is handled by the pulse debounce in coinInterrupt().
  if (relayChanged && allowed) {
    // An old cutoff timer must not suppress the first coin after re-enabling.
    ignoreCoinPulsesUntil = 0;
  } else if (relayChanged && !allowed) {
    ignoreCoinPulsesUntil = millis() + 600;
  }
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
  // Short debounce: filters contact noise while capturing fast pulse bursts
  // from ?1 (1 pulse), ?5 (5 pulses), ?10 (10 pulses), ?20 (20 pulses)
  // Coin acceptors typically send pulses 50-80ms apart within a burst.
  if (now - lastPulse >= COIN_PULSE_DEBOUNCE_MS) {
    credits++;            // Count every pulse — including the remainder of a multi-peso coin
    coinPulseReceived = true;
    lastCoinBurstTime = now;  // Track when the last pulse arrived
    lastPulse = now;

    if (credits >= maximumCreditsAllowed) {
      // Option A: Do NOT cut relay here.
      // Queue the cutoff and let loop() fire it only after 350ms of silence,
      // so all remaining pulses of the current coin are fully counted first.
      pendingCoinAcceptorOff = true;
    }
  }
}



// INDICATORS COIN
// Split from revamped_final_mega.ino for readability.

void setMachineIndicator(int state, bool sound) {
  indicatorState = static_cast<IndicatorState>(state);
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
  if (relayChanged && allowed) {
    // Ignore only the short relay-switch transient. The previous 600 ms filter
    // was long enough to discard valid denomination pulses.
    ignoreCoinPulsesUntil = millis() + 100;
  } else if (relayChanged && !allowed) {
    ignoreCoinPulsesUntil = millis() + 600;
  }
  if (!allowed) {
    noInterrupts();
    pendingCoinAcceptorOff = false;
    interrupts();
  }
}

void coinInterrupt() {
  // The deferred cutoff leaves this enabled until the pulse burst settles, so
  // there is no reason to accept pulses once the software gate is closed.
  if (orderInProgress || !coinAcceptorEnabled) return;

  unsigned long now = millis();
  // Anti-glitch: Ignore power surge / relay transient noise on Pin D2
  // (only active after MANUAL relay switching, not after coin-triggered cutoff)
  if (now < ignoreCoinPulsesUntil) return;

  static unsigned long lastPulse = 0;
  // Short debounce: filters contact noise while capturing fast pulse bursts
  // from ?1 (1 pulse), ?5 (5 pulses), ?10 (10 pulses), ?20 (20 pulses)
  // Coin acceptors typically send pulses 50-80ms apart within a burst.
  if (now - lastPulse >= COIN_PULSE_DEBOUNCE_MS) {
    if (lastPulse == 0 || now - lastPulse > COIN_BURST_SILENCE_MS) {
      coinBurstPulseCount = 0;
    }
    coinBurstPulseCount++;
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



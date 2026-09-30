// DIAGNOSTICS RESET
// Split from revamped_final_mega.ino for readability.

void runDiagnostics() {
  Serial.println();
  Serial.println("========== DIAGNOSTICS (OPTION A) ==========");
  Serial.print("Uptime: "); Serial.print(millis() / 1000); Serial.println("s");
  Serial.print("TFT (ILI9341)............ "); Serial.println(diagTftOk ? "OK" : "FAIL");
  Serial.print("Touchscreen (XPT2046)..... "); Serial.println(diagTouchOk ? "OK" : "FAIL");
  Serial.print("Coin acceptor pin (D2).... "); Serial.println("INPUT_PULLUP + external interrupt INT4 configured");
  Serial.println("Serial2 (Pins 16/17) ---> Paper Uno connected at 9600 baud");
  Serial.println("Serial3 (Pins 14/15) ---> Ballpen Uno connected at 9600 baud");

  Serial.println("============================================");
  UNO_SERIAL.println("STATUS?");
  BALLPEN_SERIAL.println("STATUS?");
}

void printHardwareStatus() {
  Serial.println("--- HARDWARE STATUS ---");
  Serial.print("Credit: P"); Serial.println(credits);
  Serial.print("Order active: "); Serial.println(orderInProgress ? "YES" : "NO");
  Serial.print("Indicator: ");
  Serial.println(indicatorState == INDICATOR_READY ? "READY (green)" : indicatorState == INDICATOR_ACTIVE ? "ACTIVE (blue)" : "ERROR (red)");
  Serial.print("Coin input D2: "); Serial.println(digitalRead(COIN_PIN) == LOW ? "LOW / pulse" : "HIGH / idle");
  Serial.print("Coin relay D3: "); Serial.println(digitalRead(COIN_INHIBIT_PIN) == coinRelayOnLevel ? "ON" : "OFF");
  Serial.print("Hopper relay D22: "); Serial.println(digitalRead(CHANGE_HOPPER_MOTOR_PIN) == HOPPER_RELAY_ON ? "ON" : "OFF");
  Serial.print("Hopper sensor D23: "); Serial.println(digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW ? "LOW / blocked" : "HIGH / clear");
  Serial.print("Paper Uno: "); Serial.println(paperUnoResponsive ? "RESPONSIVE" : "NOT CONFIRMED");
  Serial.print("Ballpen Uno: "); Serial.println(ballpenUnoResponsive ? "RESPONSIVE" : "NOT CONFIRMED");
}

void monitorControllerHealth() {
  if (millis() - controllerCheckStartedAt < CONTROLLER_READY_GRACE_MS) return;

  // Controller availability is checked when its own dispense command runs.
  // A missing optional controller must not prevent the Mega from booting,
  // accepting credits, or keeping the TFT usable.
  const unsigned long now = millis();
  if (lastPaperUnoResponseAt > 0 &&
      now - lastPaperUnoResponseAt > CONTROLLER_RESPONSE_TIMEOUT_MS) {
    paperUnoResponsive = false;
  }
  if (lastBallpenUnoResponseAt > 0 &&
      now - lastBallpenUnoResponseAt > CONTROLLER_RESPONSE_TIMEOUT_MS) {
    ballpenUnoResponsive = false;
  }

  static unsigned long nextStatusPoll = 0;
  if (millis() < nextStatusPoll) return;
  UNO_SERIAL.println("STATUS?");
  BALLPEN_SERIAL.println("STATUS?");

  if (!paperUnoResponsive) {
    CLOUD_SERIAL.println("HARDWARE_EVENT:PAPER_UNO:DISCONNECTED");
    paperUnoDisconnectObserved = true;
  } else if (paperUnoDisconnectObserved) {
    CLOUD_SERIAL.println("HARDWARE_EVENT:PAPER_UNO:CONNECTED");
    paperUnoDisconnectObserved = false;
  }

  if (!ballpenUnoResponsive) {
    CLOUD_SERIAL.println("HARDWARE_EVENT:BALLPEN_UNO:DISCONNECTED");
    ballpenUnoDisconnectObserved = true;
  } else if (ballpenUnoDisconnectObserved) {
    CLOUD_SERIAL.println("HARDWARE_EVENT:BALLPEN_UNO:CONNECTED");
    ballpenUnoDisconnectObserved = false;
  }
  nextStatusPoll = millis() + 3000;
}

void softResetMachineState() {
  Serial.println("SOFT RESET: returning machine logic to idle state.");
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  // This command is best-effort: reset must work even if Ballpen Uno is absent.
  BALLPEN_SERIAL.println("STOP");

  noInterrupts();
  credits = 0;
  coinPulseReceived = false;
  interrupts();

  isProcessing = false;
  orderInProgress = false;
  activeTransactionId = "";
  activeTrNumber = "";
  activeTransactionStatus = "";
  activeChangeDueCents = 0;
  activeChangePaidCents = 0;
  setTransactionStage(TRANSACTION_IDLE);
  changeReleaseTimedOut = false;
  selectedPaperBrand = "Budget";
  activeCatalogType = "paper";
  cartCount = 0;
  cartDispenseIndex = 0;
  orderSummaryText = "";
  orderTotalCost = 0;
  resetPendingSelections();
  setCoinAcceptance(true);

  currentScreen = SCREEN_IDLE;
  refreshMachineAvailability(true);
  updateLCD();
  tftUiSetCredits();
  // Show the reset/reconnect state immediately instead of leaving stale Wi-Fi text.
  tftUiSetWifiStatus(WIFI_STATUS_CONNECTING);
  CLOUD_SERIAL.println("CREDIT:0");
  CLOUD_SERIAL.println("SOFT_RESET");
  CLOUD_SERIAL.println("STATUS?");
  CLOUD_SERIAL.println("GET_CATALOG");
  UNO_SERIAL.println("STATUS?");
  BALLPEN_SERIAL.println("STATUS?");
  Serial.println("SOFT RESET: done.");
}


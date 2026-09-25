// DIAGNOSTICS RESET
// Split from revamped_final_mega.ino for readability.

void runDiagnostics() {
  Serial.println();
  Serial.println("========== DIAGNOSTICS (OPTION A) ==========");
  Serial.print("Uptime: "); Serial.print(millis() / 1000); Serial.println("s");
  Serial.print("TFT (ILI9341)............ "); Serial.println(diagTftOk ? "OK" : "FAIL");
  Serial.print("Touchscreen (XPT2046)..... "); Serial.println(diagTouchOk ? "OK" : "FAIL");
  Serial.print("Coin acceptor pin (D2).... "); Serial.println("INPUT_PULLUP + interrupt INT0 configured");
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
  Serial.print("Hopper relay D22: "); Serial.println(digitalRead(CHANGE_HOPPER_MOTOR_PIN) == HOPPER_RELAY_ON ? "ON" : "OFF");
  Serial.print("Hopper sensor D23: "); Serial.println(digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW ? "LOW / blocked" : "HIGH / clear");
  Serial.print("Paper Uno: "); Serial.println(paperUnoResponsive ? "RESPONSIVE" : "NOT CONFIRMED");
  Serial.print("Ballpen Uno: "); Serial.println(ballpenUnoResponsive ? "RESPONSIVE" : "NOT CONFIRMED");
}

void monitorControllerHealth() {
  if (millis() - controllerCheckStartedAt < CONTROLLER_READY_GRACE_MS) return;

  String missing = "";
  if (!paperUnoResponsive) missing = "Paper Uno disconnected";
  if (!ballpenUnoResponsive) {
    if (missing.length()) missing += " / ";
    missing += "Ballpen Uno disconnected";
  }

  if (missing.length()) {
    if (hardwareFaultMessage != missing) {
      hardwareFaultMessage = missing;
      setCoinAcceptance(false);
      currentScreen = SCREEN_IDLE;
      redrawCurrentScreen();
      nextHardwareFaultBeepAt = 0;
    }
    if (millis() >= nextHardwareFaultBeepAt) {
      setMachineIndicator(INDICATOR_ERROR, true);
      UNO_SERIAL.println("STATUS?");
      BALLPEN_SERIAL.println("STATUS?");
      nextHardwareFaultBeepAt = millis() + HARDWARE_FAULT_BEEP_INTERVAL_MS;
    }
    return;
  }

  if (hardwareFaultMessage.length()) {
    hardwareFaultMessage = "";
    setCoinAcceptance(credits < maximumCreditsAllowed && !orderInProgress);
    refreshMachineAvailability(false);
    redrawCurrentScreen();
  }
}

void softResetMachineState() {
  Serial.println("SOFT RESET: returning machine logic to idle state.");
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
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
  redrawCurrentScreen();
  CLOUD_SERIAL.println("CREDIT:0");
  CLOUD_SERIAL.println("SOFT_RESET");
  CLOUD_SERIAL.println("STATUS?");
  delay(300);
  CLOUD_SERIAL.println("GET_CATALOG");
  UNO_SERIAL.println("STATUS?");
  BALLPEN_SERIAL.println("STATUS?");
  Serial.println("SOFT RESET: done.");
}


// DIAGNOSTICS RESET
// Split from revamped_final_mega.ino for readability.

void runDiagnostics() {
  Serial.println();
  Serial.println("========== DIAGNOSTICS (OPTION A) ==========");
  Serial.print("Uptime: "); Serial.print(millis() / 1000); Serial.println("s");
  Serial.print("OLED (SH1106)............ "); Serial.println(diagOledOk ? "OK" : "FAIL");
  Serial.print("Touchscreen (XPT2046)..... "); Serial.println(diagTouchOk ? "OK" : "FAIL");
  Serial.print("Coin acceptor pin (D2).... "); Serial.println("INPUT_PULLUP + interrupt INT0 configured");
  Serial.println("Serial2 (Pins 16/17) ---> Arduino Uno Paper Controller connected at 9600 baud");

  Serial.println("--- Pen IR sensors (active LOW = beam broken) ---");
  for (int i = 0; i < BALLPEN_COUNT; i++) {
    Serial.print("  Slot "); Serial.print(i + 1); Serial.print(" (D");
    Serial.print(penIrPins[i]); Serial.print("): ");
    Serial.println(digitalRead(penIrPins[i]) == HIGH ? "OK - beam clear" : "WARNING - LOW at idle");
  }

  Serial.println("============================================");
  UNO_SERIAL.println("STATUS?");
}

void printHardwareStatus() {
  Serial.println("--- HARDWARE STATUS ---");
  Serial.print("Credit: P"); Serial.println(credits);
  Serial.print("Order active: "); Serial.println(orderInProgress ? "YES" : "NO");
  Serial.print("Indicator: ");
  Serial.println(indicatorState == INDICATOR_READY ? "READY (green)" : indicatorState == INDICATOR_ACTIVE ? "ACTIVE (blue)" : "ERROR (red)");
  for (int i = 0; i < BALLPEN_COUNT; i++) {
    Serial.print("Pen IR "); Serial.print(i + 1); Serial.print(" (D");
    Serial.print(penIrPins[i]); Serial.print("): ");
    Serial.println(digitalRead(penIrPins[i]) == LOW ? "LOW / blocked" : "HIGH / clear");
  }
  Serial.print("Hopper relay D14: "); Serial.println(digitalRead(CHANGE_HOPPER_MOTOR_PIN) == HOPPER_RELAY_ON ? "ON" : "OFF");
  Serial.print("Hopper sensor D15: "); Serial.println(digitalRead(CHANGE_HOPPER_SENSOR_PIN) == LOW ? "LOW / blocked" : "HIGH / clear");
}

void softResetMachineState() {
  Serial.println("SOFT RESET: returning machine logic to idle state.");
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  for (int i = 0; i < BALLPEN_COUNT; i++) stopStepper(i);

  noInterrupts();
  credits = 0;
  coinPulseReceived = false;
  interrupts();

  isProcessing = false;
  orderInProgress = false;
  activeTransactionId = "";
  activeTrNumber = "";
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

  servoChange.write(0);
  servoPen.write(0);

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
  Serial.println("SOFT RESET: done.");
}


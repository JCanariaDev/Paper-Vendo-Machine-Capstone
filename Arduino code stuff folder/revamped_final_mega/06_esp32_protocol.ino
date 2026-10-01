// ESP32 PROTOCOL
// Split from revamped_final_mega.ino for readability.

void startOrder() {
  if (ballpenCartQuantity() > maximumBallpensPerTransaction) {
    tftUiShowError("Max " + String(maximumBallpensPerTransaction) + " ballpens");
    return;
  }
  if (ballpenCartQuantity() > 0 && ballpenCartQuantity() < minimumBallpensPerTransaction) {
    tftUiShowError("Minimum " + String(minimumBallpensPerTransaction) + " ballpens");
    return;
  }
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  orderInProgress = true;
  setMachineIndicator(INDICATOR_ACTIVE, true);
  setCoinAcceptance(false);
  orderSummaryText = "";
  orderTotalCost = cartTotal();
  for (int i = 0; i < cartCount; i++) {
    if (orderSummaryText.length()) orderSummaryText += "\n";
    orderSummaryText += cart[i].name + " x" + String(cart[i].qty) +
                        "  P" + String(cart[i].price * cart[i].qty, 2);
  }
  dispenseResultSummary = "";
  activeTransactionId = "";
  activeTrNumber = "";
  activeTransactionStatus = "";
  activeChangeDueCents = 0;
  activeChangePaidCents = 0;
  changeReleaseTimedOut = false;
  currentScreen = SCREEN_SUMMARY;
  setTransactionStage(TRANSACTION_CHECKOUT);
  drawSummaryScreen();

  String encodedLines = "";
  for (int i = 0; i < cartCount; i++) {
    if (encodedLines.length()) encodedLines += ';';
    encodedLines += cart[i].type + "," + String(cart[i].id) + "," + String(cart[i].qty);
  }
  CLOUD_SERIAL.println("CHECKOUT:" + String((unsigned long)credits * 100UL) + ":" + encodedLines);
}

void executeDispensePlan(String message) {
  if (!orderInProgress || transactionStage != TRANSACTION_CHECKOUT) {
    Serial.println("Ignoring stale dispense plan received outside checkout stage.");
    return;
  }

  // A paper request can contain several sheets, and Paper Uno allows up to
  // 20 seconds per sheet while waiting for the exit sensor. The old 45-second
  // whole-plan limit could abort a valid multi-sheet order while the Uno was
  // still dispensing. Keep a generous final safety limit, while each device
  // still has its own shorter timeout and reports partial output accurately.
  const unsigned long DISPENSE_PLAN_TIMEOUT_MS = 180000UL;
  const unsigned long planStartedAt = millis();

  // Format: PLAN:<tx_id>:<tr_number>:<subtotal_cents>:<change_due_cents>:<encodedPlan>
  int p1 = message.indexOf(':');
  int p2 = message.indexOf(':', p1 + 1);
  int p3 = message.indexOf(':', p2 + 1);
  int p4 = message.indexOf(':', p3 + 1);
  
  String transactionId = "";
  String trNumber = "";
  int subtotalCents = 0;
  int changeDueCents = 0;
  String encodedPlan = "";

  if (p4 > 0) {
    transactionId = message.substring(p1 + 1, p2);
    trNumber = message.substring(p2 + 1, p3);
    subtotalCents = message.substring(p3 + 1, p4).toInt();
    int p5 = message.indexOf(':', p4 + 1);
    if (p5 > 0) {
      changeDueCents = message.substring(p4 + 1, p5).toInt();
      encodedPlan = message.substring(p5 + 1);
    } else {
      changeDueCents = message.substring(p4 + 1).toInt();
    }
  } else if (p2 > 0) {
    // Fallback for legacy 2-part format
    transactionId = message.substring(p1 + 1, p2);
    encodedPlan = message.substring(p2 + 1);
    trNumber = "TR-00000";
  }

  activeTransactionId = transactionId;
  activeTrNumber = trNumber;
  activeChangeDueCents = changeDueCents;
  activeChangePaidCents = 0;
  orderTotalCost = subtotalCents / 100.0;
  setTransactionStage(TRANSACTION_DISPENSING);

  // Render Status Bar (displays TR Number on top-left)
  drawTftStatusBar();

  // Show "Dispensing items..." on TFT
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Dispensing Items...", tft.width() / 2, 130);
  tft.setTextSize(1);
  tft.setTextColor(COL_ORANGE);
  printCentered("Please wait for your paper / pens", tft.width() / 2, 165);

  // -- STEP 1: GUARANTEED PRODUCT-FIRST PHYSICAL DISPENSING --
  String results = "";
  bool dispenseFailed = false;
  int start = 0;
  while (start < encodedPlan.length()) {
    if (millis() - planStartedAt >= DISPENSE_PLAN_TIMEOUT_MS) {
      Serial.println("Dispense plan timeout; submitting partial results.");
      break;
    }

    int end = encodedPlan.indexOf(';', start);
    String line = end < 0 ? encodedPlan.substring(start) : encodedPlan.substring(start, end);
    int c1 = line.indexOf(',');
    int c2 = line.indexOf(',', c1 + 1);
    int c3 = line.indexOf(',', c2 + 1);
    if (c1 <= 0 || c2 <= c1 || c3 <= c2) break;
    String type = line.substring(0, c1);
    int productId = line.substring(c1 + 1, c2).toInt();
    int channel = line.substring(c2 + 1, c3).toInt();
    int expectedOutput = line.substring(c3 + 1).toInt();
    int actualOutput = 0;

    if (type == "paper") {
      // Delegate paper dispense to Arduino Uno.
      actualOutput = dispensePaperFromUno(channel, expectedOutput,
                                           paperCatalog[channel - 1].name);
    } else {
      // Delegate ballpen dispense to the dedicated Ballpen Uno.
      actualOutput = dispensePenFromUno(channel, expectedOutput);
    }

    if (actualOutput < expectedOutput) dispenseFailed = true;

    if (dispenseResultSummary.length()) dispenseResultSummary += "\n";
    String resultLabel = actualOutput >= expectedOutput ? "OK " : "FAILED ";
    String resultType = type == "pen" ? "Ballpen" : "Paper";
    dispenseResultSummary += resultLabel + resultType + " " +
                             String(actualOutput) + "/" + String(expectedOutput);

    if (results.length()) results += ';';
    results += type + "," + String(productId) + "," + String(actualOutput);
    if (end < 0) break;
    start = end + 1;
  }

  // -- STEP 2: ALWAYS ATTEMPT CHANGE RELEASE AFTER PHYSICAL DISPENSING --
  // A failed or partial item dispense must not trap the customer's remaining
  // credits. The item result is still recorded as failed/partial, while the
  // hopper gets a chance to release the change that remains due.
  if (millis() - planStartedAt >= DISPENSE_PLAN_TIMEOUT_MS) {
    Serial.println("Skipping change release after dispense plan timeout.");
    activeChangePaidCents = 0;
  } else if (activeChangeDueCents > 0) {
    setTransactionStage(TRANSACTION_RELEASING_CHANGE);
    tft.fillRect(0, 110, tft.width(), 80, COL_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(COL_WHITE);
    printCentered("Releasing Change...", tft.width() / 2, 130);
    int verifiedChange = releaseVerifiedChange(activeChangeDueCents);
    activeChangePaidCents = max(0, verifiedChange);
  } else {
    activeChangePaidCents = 0;
  }

  // -- STEP 3: NOTIFY ESP32 CLOUD GATEWAY --
  CLOUD_SERIAL.println("FINISH:" + activeTransactionId + ":" + results + ":" +
                       String(activeChangePaidCents) + ":" +
                       String(changeReleaseTimedOut ? 1 : 0));

  setTransactionStage(TRANSACTION_FINALIZING);

  // The hopper is optional. If it is disconnected, do not leave the machine
  // waiting for a FINISHED response forever after the 20-second hopper wait.
  // The ESP32 can still retry the database finish in the background.
  if (changeReleaseTimedOut) {
    localFinishFallbackActive = true;
    localFinishFallbackTransactionId = activeTransactionId;
    credits = 0;
    orderInProgress = false;
    setCoinAcceptance(false);
    cartCount = 0;
    activeTransactionStatus = "PARTIAL_SUCCESS";
    currentScreen = SCREEN_RECEIPT;
    setTransactionStage(TRANSACTION_RECEIPT);
    updateLCD();
    refreshMachineAvailability(true);
    drawReceiptScreen();
    Serial.println("Hopper timeout: continuing locally; remaining change is recorded as owed.");
  }
}

void setTransactionStage(TransactionStage stage) {
  transactionStage = stage;
  transactionStageStartedAt = millis();
}

void finalizeTransactionLocally(const String &reason) {
  if (!orderInProgress) return;

  localFinishFallbackActive = true;
  localFinishFallbackTransactionId = activeTransactionId;
  credits = 0;
  orderInProgress = false;
  setCoinAcceptance(false);
  cartCount = 0;
  activeTransactionStatus = "PARTIAL_SUCCESS";
  currentScreen = SCREEN_RECEIPT;
  setTransactionStage(TRANSACTION_RECEIPT);
  updateLCD();
  refreshMachineAvailability(true);
  drawReceiptScreen();
  Serial.println("Transaction watchdog finalized locally: " + reason);
  CLOUD_SERIAL.println("STAGE_ERROR:FINALIZATION:" + reason);
}

void monitorTransactionWatchdog() {
  if (!orderInProgress) return;
  const unsigned long elapsed = millis() - transactionStageStartedAt;

  if (transactionStage == TRANSACTION_CHECKOUT &&
      elapsed >= CHECKOUT_RESPONSE_TIMEOUT_MS) {
    Serial.println("Transaction watchdog: checkout response timed out.");
    CLOUD_SERIAL.println("STAGE_ERROR:CHECKOUT:ESP32_OR_DATABASE_TIMEOUT");
    showError("Checkout unavailable");
    setTransactionStage(TRANSACTION_IDLE);
  } else if (transactionStage == TRANSACTION_FINALIZING &&
             elapsed >= FINALIZATION_STAGE_TIMEOUT_MS) {
    finalizeTransactionLocally("ESP32_FINISH_ACK_TIMEOUT");
  }
}

void finishUiAfterTransaction(String message) {
  // Format: FINISHED:<tx_id>:<tr_number>:<status>:<change_due>:<change_paid>
  int p1 = message.indexOf(':');
  int p2 = message.indexOf(':', p1 + 1);
  int p3 = message.indexOf(':', p2 + 1);
  int p4 = message.indexOf(':', p3 + 1);
  int p5 = message.indexOf(':', p4 + 1);

  // A late backend response for a transaction already finalized locally after
  // a hopper timeout must not overwrite the next customer's screen state.
  if (p2 > 0) {
    String finishedTransactionId = message.substring(p1 + 1, p2);
    if (localFinishFallbackActive &&
        finishedTransactionId == localFinishFallbackTransactionId) {
      // The UI already moved on locally, but the ESP32 must be allowed to
      // drain its queued completion and process the next transaction.
      CLOUD_SERIAL.println("FINISHED_ACK:" + finishedTransactionId);
      return;
    }
  }

  String trNum = activeTrNumber;
  String transactionStatus = "COMPLETED";
  int dueCents = activeChangeDueCents;
  int paidCents = activeChangePaidCents;

  if (p2 > 0) {
    if (p3 > 0) trNum = message.substring(p2 + 1, p3);
    if (p4 > 0) transactionStatus = message.substring(p3 + 1, p4);
    if (p5 > 0) {
      dueCents = message.substring(p4 + 1, p5).toInt();
      paidCents = message.substring(p5 + 1).toInt();
    }
  }

  credits = 0;
  orderInProgress = false;
  setCoinAcceptance(false);
  cartCount = 0;
  updateLCD();
  refreshMachineAvailability(true);

  activeTrNumber = trNum;
  activeTransactionStatus = transactionStatus;
  activeChangeDueCents = dueCents;
  activeChangePaidCents = paidCents;
  changeReleaseTimedOut = false;
  setTransactionStage(TRANSACTION_RECEIPT);

  BALLPEN_SERIAL.println("BEEP:1000:300");

  // Switch to non-blocking Receipt Screen with CONFIRM button
  currentScreen = SCREEN_RECEIPT;
  drawReceiptScreen();
  CLOUD_SERIAL.println("FINISHED_ACK:" + activeTransactionId);
}

void handleCloudCommand(String msg) {
  if (msg == "CHECKOUT_RECEIVED") {
    // The ESP32 has accepted the checkout request. Restart the single
    // communication watchdog from this acknowledgement.
    if (transactionStage == TRANSACTION_CHECKOUT) {
      transactionStageStartedAt = millis();
      Serial.println("ESP32 acknowledged checkout request.");
    }
  }
  else if (msg.startsWith("PLAN:")) executeDispensePlan(msg);
  else if (msg.startsWith("FINISHED:")) finishUiAfterTransaction(msg);
  else if (msg.startsWith("ERR:")) {
    String error = msg.substring(4);
    Serial.println("ESP32 error: " + error);
    if (error == "FINISH_PENDING") {
      // A finish retry is a background database-sync notice, not a checkout
      // failure; do not cancel the active order or dismiss its receipt.
      Serial.println("Transaction completion is queued for database retry.");
    } else if (error == "FINISH_RETRY_FAILED" || error == "FINISH_QUEUE_FULL") {
      tftUiShowError("Record sync pending");
    } else {
      showError(error);
    }
  }
  // -- Dynamic catalog sync from ESP32 --------------------------
  else if (msg.startsWith("PAPER_BAY:")) parsePaperBay(msg);
  else if (msg.startsWith("PEN_BAY:"))   parsePenBay(msg);
  else if (msg.startsWith("OPTIONS:"))   parseMachineOptions(msg);
  // -------------------------------------------------------------
  else if (msg.startsWith("WIFI:")) {
    bool connected = msg.substring(5) == "1";
    Serial.println(connected ? "ESP32 WiFi status: CONNECTED" : "ESP32 WiFi status: DISCONNECTED");
    tftUiSetWifiConnected(connected);
  }
  else if (msg == "WIFISTATE:CONNECTING") {
    tftUiSetWifiStatus(WIFI_STATUS_CONNECTING);
  }
  else if (msg == "WIFISTATE:NOTFOUND") {
    tftUiSetWifiStatus(WIFI_STATUS_NOT_FOUND);
  }
}


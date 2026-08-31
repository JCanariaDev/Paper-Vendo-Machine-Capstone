// ESP32 PROTOCOL
// Split from revamped_final_mega.ino for readability.

void startOrder() {
  digitalWrite(CHANGE_HOPPER_MOTOR_PIN, HOPPER_RELAY_OFF);
  hopperManualRunning = false;
  orderInProgress = true;
  setMachineIndicator(INDICATOR_ACTIVE, true);
  setCoinAcceptance(false);
  orderSummaryText = "";
  orderTotalCost = 0;
  activeTransactionId = "";
  activeTrNumber = "";
  activeChangeDueCents = 0;
  activeChangePaidCents = 0;
  currentScreen = SCREEN_SUMMARY;
  drawSummaryScreen();

  String encodedLines = "";
  for (int i = 0; i < cartCount; i++) {
    if (encodedLines.length()) encodedLines += ';';
    encodedLines += cart[i].type + "," + String(cart[i].id) + "," + String(cart[i].qty);
  }
  CLOUD_SERIAL.println("RESERVE:" + String((unsigned long)credits * 100UL) + ":" + encodedLines);
}

void executeDispensePlan(String message) {
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
  int start = 0;
  while (start < encodedPlan.length()) {
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
      actualOutput = dispensePaperFromUno(channel, expectedOutput);
    } else {
      // Dispense pens directly on Mega
      for (int item = 0; item < expectedOutput; item++) {
        bool released = dispenseOnePen(channel);
        if (!released) break;
        actualOutput++;
      }
    }

    if (results.length()) results += ';';
    results += type + "," + String(productId) + "," + String(actualOutput);
    if (end < 0) break;
    start = end + 1;
  }

  // -- STEP 2: NON-BLOCKING COIN HOPPER CHANGE ATTEMPT --
  if (activeChangeDueCents > 0) {
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
  CLOUD_SERIAL.println("FINISH:" + activeTransactionId + ":" + results + ":" + String(activeChangePaidCents));
}

void beginReservedTransaction(String message) {
  // Legacy handler kept for compatibility
  int first = message.indexOf(':');
  int second = message.indexOf(':', first + 1);
  if (first < 0 || second < 0) return;
  activeTransactionId = message.substring(first + 1, second);
}

void finishUiAfterTransaction(String message) {
  // Format: FINISHED:<tx_id>:<tr_number>:<status>:<change_due>:<change_paid>
  int p1 = message.indexOf(':');
  int p2 = message.indexOf(':', p1 + 1);
  int p3 = message.indexOf(':', p2 + 1);
  int p4 = message.indexOf(':', p3 + 1);
  int p5 = message.indexOf(':', p4 + 1);

  String trNum = activeTrNumber;
  int dueCents = activeChangeDueCents;
  int paidCents = activeChangePaidCents;

  if (p2 > 0) {
    if (p3 > 0) trNum = message.substring(p2 + 1, p3);
    if (p5 > 0) {
      dueCents = message.substring(p4 + 1, p5).toInt();
      paidCents = message.substring(p5 + 1).toInt();
    }
  }

  credits = 0;
  orderInProgress = false;
  setCoinAcceptance(true);
  cartCount = 0;
  updateLCD();
  refreshMachineAvailability(true);

  activeTrNumber = trNum;
  activeChangeDueCents = dueCents;
  activeChangePaidCents = paidCents;

  tone(BUZZER_PIN, 1000, 300);

  // Switch to non-blocking Receipt Screen with CONFIRM button
  currentScreen = SCREEN_RECEIPT;
  drawReceiptScreen();
}

void handleCloudCommand(String msg) {
  if (msg.startsWith("RESERVED:")) beginReservedTransaction(msg);
  else if (msg.startsWith("PLAN:")) executeDispensePlan(msg);
  else if (msg.startsWith("FINISHED:")) finishUiAfterTransaction(msg);
  else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  // -- Dynamic catalog sync from ESP32 --------------------------
  else if (msg.startsWith("PAPER_BAY:")) parsePaperBay(msg);
  else if (msg.startsWith("PEN_BAY:"))   parsePenBay(msg);
  // -------------------------------------------------------------
  else if (msg.startsWith("WIFI:")) {
    bool connected = msg.substring(5) == "1";
    tftUiSetWifiConnected(connected);
  }
  else if (msg == "WIFISTATE:CONNECTING") {
    tftUiSetWifiStatus(WIFI_STATUS_CONNECTING);
  }
  else if (msg == "WIFISTATE:NOTFOUND") {
    tftUiSetWifiStatus(WIFI_STATUS_NOT_FOUND);
  }
}


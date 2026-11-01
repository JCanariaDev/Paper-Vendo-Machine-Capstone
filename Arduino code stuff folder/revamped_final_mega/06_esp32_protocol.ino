// ESP32 PROTOCOL
// Split from revamped_final_mega.ino for readability.

void handleSerialOrderCommand(String cmd) {
  if (!uiWifiConnected || !catalogLoaded) {
    Serial.println("SERIAL ORDER refused: wait for Wi-Fi and catalog sync.");
    return;
  }
  if (orderInProgress || pendingCoinAcceptorOff || cartCount != 0) {
    Serial.println("SERIAL ORDER refused: finish current activity and empty the cart first.");
    return;
  }

  String args = cmd.substring(6);
  args.trim();
  int firstSpace = args.indexOf(' ');
  int secondSpace = firstSpace < 0 ? -1 : args.indexOf(' ', firstSpace + 1);
  if (firstSpace < 0 || secondSpace < 0 || args.indexOf(' ', secondSpace + 1) >= 0) {
    Serial.println("Usage: ORDER PAPER <bay> <qty> | ORDER PEN <bay> <qty>");
    return;
  }

  String type = args.substring(0, firstSpace);
  String bayText = args.substring(firstSpace + 1, secondSpace);
  String qtyText = args.substring(secondSpace + 1);
  int bay = bayText.toInt();
  int qty = qtyText.toInt();
  if (String(bay) != bayText || String(qty) != qtyText || qty < 1 || qty > 20) {
    Serial.println("SERIAL ORDER refused: bay and quantity must be whole numbers; quantity max is 20.");
    return;
  }

  CatalogItem* item = nullptr;
  if (type == "PAPER" && bay >= 1 && bay <= PAPER_COUNT) {
    item = &paperCatalog[bay - 1];
    type = "paper";
  } else if (type == "PEN" && bay >= 1 && bay <= BALLPEN_COUNT) {
    item = &ballpenCatalog[bay - 1];
    type = "pen";
  }
  if (!item || !item->isPaperPresent || item->price <= 0) {
    Serial.println("SERIAL ORDER refused: invalid or unavailable bay.");
    return;
  }
  if (type == "pen" &&
      (qty < minimumBallpensPerTransaction || qty > maximumBallpensPerTransaction)) {
    Serial.print("SERIAL ORDER refused: pen quantity must be ");
    Serial.print(minimumBallpensPerTransaction);
    Serial.print("-");
    Serial.println(maximumBallpensPerTransaction);
    return;
  }

  float total = item->price * qty;
  if (credits < minimumCreditsToStart || credits < total) {
    Serial.println("SERIAL ORDER refused: insufficient machine credits.");
    return;
  }

  addToCart(type, item->id, item->name, item->price, qty);
  if (cartCount != 1) {
    cartCount = 0;
    Serial.println("SERIAL ORDER refused: could not add the item to the cart.");
    return;
  }

  Serial.print("SERIAL ORDER: submitting through normal checkout; item total=P");
  Serial.println(total, 2);
  Serial.println("This creates a normal cloud transaction and may release change.");
  startOrder();
}

void startOrder() {
  if (ballpenCartQuantity() > maximumBallpensPerTransaction) {
    tftUiShowError("Max " + String(maximumBallpensPerTransaction) + " ballpens");
    return;
  }
  if (ballpenCartQuantity() > 0 && ballpenCartQuantity() < minimumBallpensPerTransaction) {
    tftUiShowError("Minimum " + String(minimumBallpensPerTransaction) + " ballpens");
    return;
  }
  hopperSensorMonitorEnabled = false;
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
  const unsigned long DISPENSE_PLAN_TIMEOUT_MS = 60000;
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
  const long acceptedCreditCents = (long)credits * 100L;
  long locallyCalculatedChangeCents = acceptedCreditCents - subtotalCents;
  if (locallyCalculatedChangeCents < 0) locallyCalculatedChangeCents = 0;
  const bool changeAmountConsistent = locallyCalculatedChangeCents == changeDueCents;
  Serial.print("CHANGE CHECK: credit=P");
  Serial.print(acceptedCreditCents / 100.0, 2);
  Serial.print(" subtotal=P");
  Serial.print(subtotalCents / 100.0, 2);
  Serial.print(" calculated due=P");
  Serial.print(locallyCalculatedChangeCents / 100.0, 2);
  Serial.print(" server due=P");
  Serial.print(changeDueCents / 100.0, 2);
  Serial.print(" expected one-peso coins=");
  Serial.println(changeDueCents / 100);
  if (!changeAmountConsistent) {
    Serial.println("CHANGE WARNING: local and server amounts differ; hopper release will be skipped for safety.");
  }
  activeChangeDueCents = changeDueCents;
  activeChangePaidCents = 0;
  orderTotalCost = subtotalCents / 100.0;

  // Transition screen state so summary animation stops completely
  currentScreen = SCREEN_DISPENSING;
  drawDispensingScreen();

  // -- STEP 1: GUARANTEED PRODUCT-FIRST PHYSICAL DISPENSING --
  String results = "";
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

  // Show clear status on screen if any physical dispense failed
  if (dispenseResultSummary.indexOf("FAILED") >= 0) {
    tft.fillRect(0, 110, tft.width(), 80, COL_BACKGROUND);
    printCenteredStyled("Dispense Failed!", tft.width() / 2, 130, COL_RED, true, true);
    printCenteredStyled("Processing refund / change...", tft.width() / 2, 160, COL_TEXT);
  }

  // -- STEP 2: NON-BLOCKING COIN HOPPER CHANGE ATTEMPT --
  if (millis() - planStartedAt >= DISPENSE_PLAN_TIMEOUT_MS) {
    Serial.println("Skipping change release after dispense plan timeout.");
    activeChangePaidCents = 0;
  } else if (activeChangeDueCents > 0 && !changeAmountConsistent) {
    activeChangePaidCents = 0;
  } else if (activeChangeDueCents > 0) {
    delay(200); // Allow 12V power rail to settle after NEMA motor de-energization
    tft.fillRect(0, 110, tft.width(), 80, COL_BACKGROUND);
    printCenteredStyled("Releasing Change...", tft.width() / 2, 130, COL_TEXT, true, true);
    int verifiedChange = releaseVerifiedChange(activeChangeDueCents);
    activeChangePaidCents = max(0, verifiedChange);
  } else {
    activeChangePaidCents = 0;
  }

  // -- STEP 3: NOTIFY ESP32 CLOUD GATEWAY --
  CLOUD_SERIAL.println("FINISH:" + activeTransactionId + ":" + results + ":" + String(activeChangePaidCents));
  finishSentAt = millis(); // Start safety timeout for FINISHED: response
}

void beginReservedTransaction(String message) {
  // Legacy handler kept for compatibility
  int first = message.indexOf(':');
  int second = message.indexOf(':', first + 1);
  if (first < 0 || second < 0) return;
  activeTransactionId = message.substring(first + 1, second);
}

void finishUiAfterTransaction(String message) {
  finishSentAt = 0; // Cancel safety timeout — response received
  // Format: FINISHED:<tx_id>:<tr_number>:<status>:<change_due>:<change_paid>
  int p1 = message.indexOf(':');
  int p2 = message.indexOf(':', p1 + 1);
  int p3 = message.indexOf(':', p2 + 1);
  int p4 = message.indexOf(':', p3 + 1);
  int p5 = message.indexOf(':', p4 + 1);

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
  setCoinAcceptance(true);
  cartCount = 0;
  updateLCD();
  refreshMachineAvailability(true);

  activeTrNumber = trNum;
  activeTransactionStatus = transactionStatus;
  activeChangeDueCents = dueCents;
  activeChangePaidCents = paidCents;

  BALLPEN_SERIAL.println("BEEP:1000:300");

  // Switch to non-blocking Receipt Screen with CONFIRM button
  currentScreen = SCREEN_RECEIPT;
  drawReceiptScreen();
}

void onCatalogSyncCompleted() {
  catalogLoaded = true;
  catalogLoadingError = false;
  Serial.println("CATALOG SYNC COMPLETE: All bays and options loaded.");
  Serial.print("  Paper bays: ");
  for (int i = 0; i < PAPER_COUNT; i++) {
    Serial.print(paperBaysLoaded[i] ? "OK " : "MISS ");
  }
  Serial.print(" | Pen bays: ");
  for (int i = 0; i < BALLPEN_COUNT; i++) {
    Serial.print(penBaysLoaded[i] ? "OK " : "MISS ");
  }
  Serial.print(" | Options: ");
  Serial.println(machineOptionsLoaded ? "OK" : "MISS");

  // Now that catalog is ready, enable coin acceptor if WiFi is connected
  if (uiWifiConnected && !orderInProgress && credits < maximumCreditsAllowed) {
    setCoinAcceptance(true);
  }
  refreshMachineAvailability(true);
  if (currentScreen == SCREEN_IDLE || currentScreen == SCREEN_MAIN) {
    redrawCurrentScreen();
  }
}

void onCatalogSyncFailed() {
  catalogLoadingError = true;
  Serial.println("CATALOG SYNC FAILED: One or more fetches did not succeed.");
  if (currentScreen == SCREEN_IDLE) {
    redrawCurrentScreen();
  }
}

void handleCloudCommand(String msg) {
  if (msg.startsWith("RESERVED:")) beginReservedTransaction(msg);
  else if (msg.startsWith("PLAN:")) executeDispensePlan(msg);
  else if (msg.startsWith("FINISHED:")) finishUiAfterTransaction(msg);
  else if (msg.startsWith("ERR:FINISH_PENDING") || msg.startsWith("ERR:FINISH_RETRY_FAILED")) {
    // The ESP32 could not finalise the transaction in Supabase.
    // Instead of the generic showError() (which dumps to SCREEN_MAIN and
    // destroys all receipt context), force the receipt screen so the
    // customer still sees their TR number and dispense results.
    Serial.println("Cloud finish error: " + msg);
    if (finishSentAt > 0) {
      finishSentAt = 0;
      finishUiAfterTransaction("FINISHED:" + activeTransactionId + ":" +
                               activeTrNumber + ":COMPLETED:" +
                               String(activeChangeDueCents) + ":" +
                               String(activeChangePaidCents));
    }
  }
  else if (msg.startsWith("ERR:")) showError(msg.substring(4));
  // -- Dynamic catalog sync from ESP32 --------------------------
  else if (msg == "CATALOG_SYNC_START") {
    paperBaysLoaded[0] = false;
    paperBaysLoaded[1] = false;
    penBaysLoaded[0] = false;
    machineOptionsLoaded = false;
    Serial.println("ESP32: Catalog sync starting...");
  }
  else if (msg.startsWith("PAPER_BAY:")) parsePaperBay(msg);
  else if (msg.startsWith("PEN_BAY:"))   parsePenBay(msg);
  else if (msg.startsWith("OPTIONS:"))   parseMachineOptions(msg);
  else if (msg == "CATALOG_SYNC_COMPLETE") onCatalogSyncCompleted();
  else if (msg == "CATALOG_SYNC_FAIL")     onCatalogSyncFailed();
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


// UNO PAPER PROTOCOL
// Split from revamped_final_mega.ino for readability.

void parsePaperBay(String msg) {
  // Strip prefix
  String data = msg.substring(10); // after "PAPER_BAY:"
  int p1 = data.indexOf(':');
  int p2 = data.indexOf(':', p1 + 1);
  int p3 = data.indexOf(':', p2 + 1);
  int p4 = data.indexOf(':', p3 + 1);
  int p5 = data.indexOf(':', p4 + 1);
  int p6 = data.indexOf(':', p5 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) return;

  int bayNum   = data.substring(0, p1).toInt();          // 1-PAPER_COUNT
  int prodId   = data.substring(p1 + 1, p2).toInt();
  String pres  = data.substring(p2 + 1, p3);             // HIGH or LOW
  int sheetsPerPad = data.substring(p3 + 1, p4).toInt();
  int currentPadStock;
  int priceCents;
  String name;
  if (p6 >= 0) {
    currentPadStock = data.substring(p4 + 1, p5).toInt();
    priceCents = data.substring(p5 + 1, p6).toInt();
    name = data.substring(p6 + 1);
  } else {
    // Legacy format fallback while the ESP32 is being updated.
    currentPadStock = (pres == "HIGH") ? 1 : 0;
    priceCents = data.substring(p4 + 1, p5).toInt();
    name = data.substring(p5 + 1);
  }
  name.trim();

  int idx = bayNum - 1;
  if (idx < 0 || idx >= PAPER_COUNT) return;

  paperCatalog[idx].id    = prodId;
  paperCatalog[idx].price = priceCents / 100.0;
  paperCatalog[idx].currentPadStock = max(0, currentPadStock);
  paperCatalog[idx].sheetsPerPad = max(1, sheetsPerPad);
  paperCatalog[idx].isPaperPresent = currentPadStock > 0;
  name.toCharArray(paperCatalogNames[idx], 32);
  paperCatalog[idx].name = paperCatalogNames[idx];
  UNO_SERIAL.println("STOCK:" + String(bayNum) + ":" + String(paperCatalog[idx].currentPadStock) + ":" + String(paperCatalog[idx].sheetsPerPad));

  Serial.print("Catalog Sync Paper Bay "); Serial.print(bayNum);
  Serial.print(": "); Serial.print(name);
  Serial.print(" P"); Serial.print(priceCents / 100.0, 2);
  Serial.print(" stock="); Serial.println(paperCatalog[idx].currentPadStock);

  if (currentScreen == SCREEN_CATALOG && activeCatalogType == "paper") {
    drawCatalogScreen();
  }
}

void parsePenBay(String msg) {
  String data = msg.substring(8); // after "PEN_BAY:"
  int p1 = data.indexOf(':');
  int p2 = data.indexOf(':', p1 + 1);
  int p3 = data.indexOf(':', p2 + 1);
  int p4 = data.indexOf(':', p3 + 1);
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return;

  int bayNum     = data.substring(0, p1).toInt();        // 1-BALLPEN_COUNT
  int prodId     = data.substring(p1 + 1, p2).toInt();
  int stock      = data.substring(p2 + 1, p3).toInt();
  int priceCents = data.substring(p3 + 1, p4).toInt();
  String name    = data.substring(p4 + 1);
  name.trim();

  int idx = bayNum - 1;
  if (idx < 0 || idx >= BALLPEN_COUNT) return;

  ballpenCatalog[idx].id    = prodId;
  ballpenCatalog[idx].price = priceCents / 100.0;
  ballpenCatalog[idx].isPaperPresent = (stock > 0); // available if stock > 0
  name.toCharArray(ballpenCatalogNames[idx], 32);
  ballpenCatalog[idx].name = ballpenCatalogNames[idx];

  Serial.print("Catalog Sync Pen Bay "); Serial.print(bayNum);
  Serial.print(": "); Serial.print(name);
  Serial.print(" P"); Serial.print(priceCents / 100.0, 2);
  Serial.print(" Stock: "); Serial.println(stock);

  if (currentScreen == SCREEN_CATALOG && activeCatalogType == "pen") {
    drawCatalogScreen();
  }
}

void handleUnoMessage(String msg) {
  msg.trim();
  if (msg.startsWith("STATUS:")) {
    // Format: STATUS:HIGH,HIGH,... for the configured paper bays
    // Uno reports the last synchronized software stock state.
    String list = msg.substring(7);
    int start = 0;
    for (int i = 0; i < PAPER_COUNT; i++) {
      int comma = list.indexOf(',', start);
      String val = (comma == -1) ? list.substring(start) : list.substring(start, comma);
      paperCatalog[i].isPaperPresent = (val == "HIGH");
      if (comma == -1) break;
      start = comma + 1;
    }
    if (currentScreen == SCREEN_CATALOG && activeCatalogType == "paper") {
      drawCatalogScreen();
    }
  }
  else if (msg.startsWith("LEVEL:")) {
    // Format: LEVEL:HIGH,LOW. This is a physical low-level warning only.
    String list = msg.substring(6);
    int start = 0;
    for (int i = 0; i < PAPER_COUNT; i++) {
      int comma = list.indexOf(',', start);
      String val = (comma == -1) ? list.substring(start) : list.substring(start, comma);
      paperCatalog[i].paperLevelHigh = (val == "HIGH");
      if (comma == -1) break;
      start = comma + 1;
    }
    if (currentScreen == SCREEN_CATALOG && activeCatalogType == "paper") {
      drawCatalogScreen();
    }
  }
}

void parseMachineOptions(String msg) {
  // Format: OPTIONS:<minimum_credits>:<maximum_credits>:<minimum_ballpen_stock>
  int first = msg.indexOf(':');
  int second = msg.indexOf(':', first + 1);
  int third = msg.indexOf(':', second + 1);
  if (first < 0 || second < 0 || third < 0) return;
  minimumCreditsToStart = constrain(msg.substring(first + 1, second).toInt(), 0, 10000);
  maximumCreditsAllowed = constrain(msg.substring(second + 1, third).toInt(), minimumCreditsToStart, 10000);
  minimumBallpenStockWarning = constrain(msg.substring(third + 1).toInt(), 0, 10000);
  Serial.print("Machine options synced: minimum credits=P");
  Serial.print(minimumCreditsToStart);
  Serial.print(", ballpen warning=");
  Serial.println(minimumBallpenStockWarning);
  tftUiSetCredits();
}

int dispensePaperFromUno(int bayNumber, int sheetCount, const String &paperName) {
  if (bayNumber < 1 || bayNumber > PAPER_COUNT) return 0;
  String command = "DISPENSE:" + String(bayNumber) + ":" + String(sheetCount) + ":" + paperName;
  Serial.print("Paper Uno <- ");
  Serial.println(command);
  UNO_SERIAL.println(command);

  unsigned long startedAt = millis();
  while (millis() - startedAt < PAPER_DISPENSE_TIMEOUT_MS) {
    if (UNO_SERIAL.available()) {
      String response = UNO_SERIAL.readStringUntil('\n');
      response.trim();
      Serial.print("Paper Uno -> ");
      Serial.println(response);
      if (response.startsWith("DONE:")) {
        // Format: DONE:<bay>:<count>
        int second = response.indexOf(':', 5);
        int count = response.substring(second + 1).toInt();
        const int sheetsPerPad = max(1, paperCatalog[bayNumber - 1].sheetsPerPad);
        const int padsUsed = (count + sheetsPerPad - 1) / sheetsPerPad;
        paperCatalog[bayNumber - 1].currentPadStock = max(
          0,
          paperCatalog[bayNumber - 1].currentPadStock - padsUsed
        );
        paperCatalog[bayNumber - 1].isPaperPresent = paperCatalog[bayNumber - 1].currentPadStock > 0;
        if (!paperCatalog[bayNumber - 1].isPaperPresent) {
          CLOUD_SERIAL.println("BAY_EMPTY:" + String(bayNumber));
        }
        return count;
      }
      else if (response.startsWith("EMPTY:")) {
        // Format: EMPTY:<bay>:<count>
        int second = response.indexOf(':', 6);
        int count = (second > 0) ? response.substring(second + 1).toInt() : 0;
        paperCatalog[bayNumber - 1].isPaperPresent = false;
        paperCatalog[bayNumber - 1].currentPadStock = 0;
        CLOUD_SERIAL.println("BAY_EMPTY:" + String(bayNumber));
        return count;
      }
    }
  }
  Serial.println("Paper Uno response timeout; no NEMA17 completion received.");
  return 0; // Timeout
}


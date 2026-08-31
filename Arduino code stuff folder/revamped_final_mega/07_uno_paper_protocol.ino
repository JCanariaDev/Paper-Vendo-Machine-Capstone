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
  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0) return;

  int bayNum   = data.substring(0, p1).toInt();          // 1-4
  int prodId   = data.substring(p1 + 1, p2).toInt();
  String pres  = data.substring(p2 + 1, p3);             // HIGH or LOW
  // p3..p4 = sheets (unused for display but kept)
  int priceCents = data.substring(p4 + 1, p5).toInt();
  String name  = data.substring(p5 + 1);
  name.trim();

  int idx = bayNum - 1;
  if (idx < 0 || idx >= PAPER_COUNT) return;

  paperCatalog[idx].id    = prodId;
  paperCatalog[idx].price = priceCents / 100.0;
  paperCatalog[idx].isPaperPresent = (pres == "HIGH");
  name.toCharArray(paperCatalogNames[idx], 32);
  paperCatalog[idx].name = paperCatalogNames[idx];

  Serial.print("Catalog Sync Paper Bay "); Serial.print(bayNum);
  Serial.print(": "); Serial.print(name);
  Serial.print(" P"); Serial.print(priceCents / 100.0, 2);
  Serial.print(" ["); Serial.print(pres); Serial.println("]");

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

  int bayNum     = data.substring(0, p1).toInt();        // 1-3
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
    // Format: STATUS:HIGH,HIGH,LOW,HIGH
    // Uno reports live L5290 sensor states; update paperCatalog presence flags
    String list = msg.substring(7);
    int start = 0;
    for (int i = 0; i < 4; i++) {
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
}

int dispensePaperFromUno(int bayNumber, int sheetCount) {
  if (bayNumber < 1 || bayNumber > 4) return 0;
  UNO_SERIAL.println("DISPENSE:" + String(bayNumber) + ":" + String(sheetCount));

  unsigned long startedAt = millis();
  while (millis() - startedAt < PAPER_DISPENSE_TIMEOUT_MS) {
    if (UNO_SERIAL.available()) {
      String response = UNO_SERIAL.readStringUntil('\n');
      response.trim();
      if (response.startsWith("DONE:")) {
        // Format: DONE:<bay>:<count>
        int second = response.indexOf(':', 5);
        int count = response.substring(second + 1).toInt();
        return count;
      }
      else if (response.startsWith("EMPTY:")) {
        // Format: EMPTY:<bay>:<count>
        int second = response.indexOf(':', 6);
        int count = (second > 0) ? response.substring(second + 1).toInt() : 0;
        paperCatalog[bayNumber - 1].isPaperPresent = false;
        CLOUD_SERIAL.println("BAY_EMPTY:" + String(bayNumber));
        return count;
      }
    }
  }
  return 0; // Timeout
}


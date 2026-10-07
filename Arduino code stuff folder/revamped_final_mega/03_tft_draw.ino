// TFT DRAW
// Split from revamped_final_mega.ino for readability.

static uint16_t blend565(uint16_t from, uint16_t to, uint8_t amount) {
  uint16_t inv = 255 - amount;
  uint8_t r = ((((from >> 11) & 0x1F) * inv) + (((to >> 11) & 0x1F) * amount)) / 255;
  uint8_t g = ((((from >> 5) & 0x3F) * inv) + (((to >> 5) & 0x3F) * amount)) / 255;
  uint8_t b = (((from & 0x1F) * inv) + ((to & 0x1F) * amount)) / 255;
  return (r << 11) | (g << 5) | b;
}

static uint16_t layoutGradientAt(uint16_t position, uint16_t length) {
  if (length < 2) return COL_GOLD;
  uint16_t midpoint = (length - 1) / 2;
  if (position <= midpoint) {
    uint8_t amount = (uint32_t)position * 255 / max((uint16_t)1, midpoint);
    return blend565(COL_GOLD, COL_CREAM, amount);
  }
  uint16_t secondLength = (length - 1) - midpoint;
  uint8_t amount = (uint32_t)(position - midpoint) * 255 / max((uint16_t)1, secondLength);
  return blend565(COL_CREAM, COL_CYAN, amount);
}

static void drawGradientHorizontal(int x, int y, int length) {
  const int maxSegments = 18;
  int segments = min(maxSegments, length);
  for (int i = 0; i < segments; i++) {
    int start = (int32_t)i * length / segments;
    int end = (int32_t)(i + 1) * length / segments;
    int segmentLength = end - start;
    int midpoint = start + segmentLength / 2;
    tft.drawFastHLine(x + start, y, segmentLength,
                      layoutGradientAt(midpoint, length));
  }
}

static void drawGradientVertical(int x, int y, int length) {
  const int maxSegments = 12;
  int segments = min(maxSegments, length);
  for (int i = 0; i < segments; i++) {
    int start = (int32_t)i * length / segments;
    int end = (int32_t)(i + 1) * length / segments;
    int segmentLength = end - start;
    int midpoint = start + segmentLength / 2;
    tft.drawFastVLine(x, y + start, segmentLength,
                      layoutGradientAt(midpoint, length));
  }
}

static void drawGradientBorder(int x, int y, int w, int h) {
  if (w < 2 || h < 2) return;
  drawGradientHorizontal(x, y, w);
  drawGradientHorizontal(x, y + h - 1, w);
  drawGradientVertical(x, y + 1, h - 2);
  drawGradientVertical(x + w - 1, y + 1, h - 2);
}

static void drawWifiIcon(int x, int y, uint16_t color) {
  tft.drawLine(x, y, x + 5, y + 4, color);
  tft.drawLine(x + 10, y, x + 5, y + 4, color);
  tft.drawLine(x + 2, y + 3, x + 5, y + 6, color);
  tft.drawLine(x + 8, y + 3, x + 5, y + 6, color);
  tft.drawPixel(x + 5, y + 9, color);
}

static void printCenteredStyled(const String &text, int cx, int cy,
                                uint16_t color, bool headline = false,
                                bool outlined = false, bool italic = false) {
  const GFXfont *font = italic ? &FreeSansBoldOblique9pt7b
                               : (headline ? &FreeSansBold12pt7b : &FreeSansBold9pt7b);
  tft.setFont(font);
  tft.setTextSize(1);
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  int16_t cursorX = cx - (x1 + w / 2);
  int16_t cursorY = cy - (y1 + h / 2);

  if (outlined) {
    tft.setTextColor(COL_TITLE_EDGE);
    tft.setCursor(cursorX - 1, cursorY); tft.print(text);
    tft.setCursor(cursorX + 1, cursorY); tft.print(text);
    tft.setCursor(cursorX, cursorY - 1); tft.print(text);
    tft.setCursor(cursorX, cursorY + 1); tft.print(text);
  }
  tft.setTextColor(color);
  tft.setCursor(cursorX, cursorY);
  tft.print(text);
  tft.setFont(NULL);
  tft.setTextSize(1);
}

static void printModernAt(const String &text, int x, int y, uint16_t color,
                          bool italic = false) {
  tft.setFont(italic ? &FreeSansBoldOblique9pt7b : &FreeSansBold9pt7b);
  tft.setTextSize(1);
  tft.setTextColor(color);
  tft.setCursor(x, y);
  tft.print(text);
  tft.setFont(NULL);
  tft.setTextSize(1);
}

void printCentered(const String &text, int cx, int cy) {
  int16_t x1, y1;
  uint16_t w, h;
  tft.getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setCursor(cx - w / 2, cy - h / 2);
  tft.print(text);
}

void printCentered(const char* text, int cx, int cy) {
  printCentered(String(text), cx, cy);
}

void drawTftStatusBar() {
  tft.fillRect(0, 0, tft.width(), 26, COL_BACKGROUND);
  drawGradientHorizontal(0, 25, tft.width());
  tft.setTextSize(1);

  tft.fillRoundRect(4, 2, 76, 21, 7, COL_STATUS_BG);
  if (activeTrNumber.length() > 0) {
    // Show official TR Record Number on top-left throughout checkout/dispense
    tft.setTextColor(COL_CREAM);
    tft.drawRoundRect(4, 2, 76, 21, 7, COL_GOLD);
    tft.setCursor(8, 8);
    tft.print(activeTrNumber);
  } else {
    uint16_t wifiColor = uiWifiConnected ? COL_GREEN : COL_RED;
    drawWifiIcon(9, 7, wifiColor);
    tft.setTextColor(wifiColor);
    tft.setCursor(25, 8);
    tft.print(uiWifiConnected ? "WIFI OK" : "WIFI --");
  }

  int remainingChangeCents = max(0, activeChangeDueCents - activeChangePaidCents);
  String creditText;
  if (currentScreen == SCREEN_RECEIPT && remainingChangeCents > 0) {
    creditText = "Left: P" + String(remainingChangeCents / 100.0, 2);
  } else {
    creditText = "Credits: P" + String((unsigned int)credits);
  }
  int16_t x1, y1; uint16_t w, h;
  tft.setFont(&FreeSansBoldOblique9pt7b);
  tft.setTextSize(1);
  tft.getTextBounds(creditText.c_str(), 0, 0, &x1, &y1, &w, &h);
  int badgeWidth = w + 12;
  int badgeX = tft.width() - badgeWidth - 4;
  tft.fillRoundRect(badgeX, 2, badgeWidth, 21, 7, COL_STATUS_BG);
  tft.drawRoundRect(badgeX, 2, badgeWidth, 21, 7, COL_GOLD);
  tft.setTextColor(COL_CREAM);
  tft.setCursor(badgeX + 6, 6 - y1);
  tft.print(creditText);
  tft.setFont(NULL);
  tft.setTextSize(1);
}

void drawIdleScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();
  if (wifiStatus == WIFI_STATUS_CONNECTED && catalogLoaded) {
    printCenteredStyled("Insert Coins", tft.width() / 2, 140, COL_TEXT, true, true);
    printCenteredStyled("to use.", tft.width() / 2, 165, COL_TEXT);
    return;
  }

  printCenteredStyled("Starting System...", tft.width() / 2, 120, COL_TEXT, true, true);
  if (wifiStatus != WIFI_STATUS_CONNECTED) {
    printCenteredStyled("Connecting to Wi-Fi / Cloud...", tft.width() / 2, 148, COL_GOLD);
  } else if (catalogLoadingError) {
    printCenteredStyled("Catalog sync error - retrying...", tft.width() / 2, 148, COL_RED);
  } else {
    printCenteredStyled("Loading Catalog & Options...", tft.width() / 2, 148, COL_GOLD);
  }
  printCenteredStyled("Coin slot disabled while booting", tft.width() / 2, 172, COL_TEXT);

  if (wifiStatus == WIFI_STATUS_NOT_FOUND) {
    printCenteredStyled("WiFi can't be detected", tft.width() / 2, 205, COL_RED);
  }
}

void drawMainScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCenteredStyled("Select an Option", tft.width() / 2, 48, COL_TEXT, true, true);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_PAPER_BTN);
  tft.setTextColor(COL_WHITE);
  printCenteredStyled("BUY PAPER", tft.width() / 2, 122, COL_TEXT);

  tft.fillRoundRect(20, 160, 200, 55, 8, COL_PEN_BTN);
  printCenteredStyled("BUY BALLPEN", tft.width() / 2, 187, COL_TEXT);

  tft.fillRoundRect(20, 225, 200, 50, 8, COL_CHECKOUT);
  printCenteredStyled("CHECKOUT", tft.width() / 2, 250, COL_TEXT);

  tft.fillRoundRect(20, 285, 200, 35, 8, COL_CART_BTN);
  tft.setTextSize(1);
  String cartLabel = cartCount > 0 ? ("VIEW CART (" + String(cartCount) + ")") : "VIEW CART (empty)";
  printCenteredStyled(cartLabel, tft.width() / 2, 285 + 17, COL_TEXT);
}

void drawPaperBrandScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCenteredStyled("Choose Paper Brand", tft.width() / 2, 55, COL_TEXT, true, true);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_PAPER_BTN);
  tft.setTextColor(COL_WHITE);
  printCenteredStyled("BUDGET", tft.width() / 2, 122, COL_TEXT);
  tft.fillRoundRect(20, 165, 200, 55, 8, COL_PEN_BTN);
  printCenteredStyled("STANDARD", tft.width() / 2, 192, COL_TEXT);
  tft.fillRoundRect(20, 275, 200, 35, 8, COL_CART_BTN);
  tft.setTextSize(1);
  printCenteredStyled("BACK", tft.width() / 2, 292, COL_TEXT);
}

void drawCatalogScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();

  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  String title = activeCatalogType == "paper" ? "Paper Options" : "Ballpen Options";
  printCenteredStyled(title, tft.width() / 2, 38, COL_TEXT, true, true);

  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int rowH = catalogRowHeight(count);
    int btnSize = catalogButtonSize(count);
    int btnY = rowY + (rowH - btnSize) / 2;
    bool selected = pendingQty[i] > 0;
    bool isAvailable = catalog[i].isPaperPresent;

    if (selected) {
      tft.fillRoundRect(4, rowY + 2, 232, rowH - 4, 8, COL_PANEL);
      drawGradientBorder(4, rowY + 2, 232, rowH - 4);
    }

    // "-" button
    tft.fillRoundRect(8, btnY, btnSize, btnSize, 8, COL_MUTED_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(2);
    printCenteredStyled("-", 8 + btnSize / 2, btnY + btnSize / 2, COL_TEXT);

    // "+" button
    int plusX = 232 - btnSize;
    tft.fillRoundRect(plusX, btnY, btnSize, btnSize, 8, isAvailable ? COL_PEN_BTN : COL_GREY);
    printCenteredStyled("+", plusX + btnSize / 2, btnY + btnSize / 2, COL_TEXT);

    // Info Column
    printModernAt(catalog[i].name, 62, rowY + 15, COL_TEXT);
    printModernAt("P" + String(catalogDisplayPrice(i), 2), 62, rowY + 30, COL_TEXT);

    if (!isAvailable) {
      printModernAt(activeCatalogType == "paper" ? "[OUT OF PAPER]" : "[OUT OF STOCK]",
                    62, rowY + 45, COL_RED);
    } else {
      char qtyBuf[16];
      sprintf(qtyBuf, "Qty: %d", pendingQty[i]);
      printModernAt(qtyBuf, 62, rowY + 45, COL_TEXT);
      if (activeCatalogType == "paper") {
        printModernAt(catalog[i].paperLevelHigh ? "Level: HIGH" : "Level: LOW",
                      125, rowY + 45,
                      catalog[i].paperLevelHigh ? COL_GREEN : COL_GOLD);
      }
    }
  }

  tft.fillRoundRect(15, 275, 95, 35, 8, COL_PEN_BTN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCenteredStyled("ADD", 15 + 47, 275 + 17, COL_TEXT);

  tft.fillRoundRect(130, 275, 95, 35, 8, COL_MUTED_RED);
  printCenteredStyled("CANCEL", 130 + 47, 275 + 17, COL_TEXT);
}

static bool summaryToggle = false;
static unsigned long lastSummaryUpdate = 0;

void updateSummaryStatusFrame(bool reset) {
  if (reset) {
    summaryToggle = false;
    lastSummaryUpdate = millis();
    return;
  }
  unsigned long now = millis();
  if (now - lastSummaryUpdate < 1000) return;
  lastSummaryUpdate = now;
  summaryToggle = !summaryToggle;
  tft.fillRect(0, 246, tft.width(), 26, COL_BACKGROUND);
  printCenteredStyled(summaryToggle ? "Loading..." : "Fetching data",
                      tft.width() / 2, 258, COL_GOLD);
}

void drawSummaryScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();

  printCenteredStyled("Order Summary", tft.width() / 2, 40, COL_TEXT, true, true);

  int summaryLineCount = 1;
  for (int i = 0; i < (int)orderSummaryText.length(); i++) {
    if (orderSummaryText.charAt(i) == '\n') summaryLineCount++;
  }
  int panelHeight = constrain(summaryLineCount * 16 + 42, 58, 170);
  tft.fillRoundRect(7, 62, 226, panelHeight, 8, COL_PANEL);
  drawGradientBorder(7, 62, 226, panelHeight);

  int y = 70;
  int start = 0;
  while (start < (int)orderSummaryText.length()) {
    int nl = orderSummaryText.indexOf('\n', start);
    String line = (nl == -1) ? orderSummaryText.substring(start) : orderSummaryText.substring(start, nl);
    if (line.length() > 0) {
      printModernAt(line, 15, y + 8, COL_TEXT);
      y += 16;
    }
    if (nl == -1) break;
    start = nl + 1;
  }

  printModernAt("Total: P" + String(orderTotalCost, 2), 15, y + 18, COL_TEXT, true);

  if (orderInProgress) {
    printCenteredStyled("Fetching data", tft.width() / 2, 258, COL_GOLD);
    updateSummaryStatusFrame(true);
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_CART_BTN);
    printCenteredStyled("PLEASE WAIT", tft.width() / 2, 275 + 20, COL_TEXT);
  } else {
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_CART_BTN);
    printCenteredStyled("ORDER CLOSED", tft.width() / 2, 275 + 20, COL_TEXT);
  }
}

void drawCartScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();

  printCenteredStyled("Your Cart", tft.width() / 2, 45, COL_TEXT, true, true);

  if (cartCount == 0) {
    printCenteredStyled("Cart is empty.", tft.width() / 2, 150, COL_TEXT);
  } else {
    int rowH = cartRowHeight();
    for (int i = 0; i < cartCount; i++) {
      int rowY = cartRowY(i);

      String line = String(cart[i].qty) + "x " + cart[i].name;
      printModernAt(line, 10, rowY + 13, COL_TEXT);
      String priceLine = "P" + String(cart[i].price, 2) + " each - P" + String(cart[i].price * cart[i].qty, 2);
      printModernAt(priceLine, 10, rowY + 29, COL_TEXT);

      int btnSize = min(rowH - 6, 40);
      int btnY = rowY + (rowH - btnSize) / 2;
      tft.fillRoundRect(196, btnY, btnSize, btnSize, 6, COL_RED);
      printCenteredStyled("X", 196 + btnSize / 2, btnY + btnSize / 2, COL_WHITE);

      if (i < cartCount - 1) {
        drawGradientHorizontal(8, rowY + rowH - 2, 224);
      }
    }

    printModernAt("Total: P" + String(cartTotal(), 2), 10, CART_BOTTOM + 19, COL_TEXT, true);
  }

  tft.fillRoundRect(20, 275, 200, 35, 8, COL_CART_BTN);
  printCenteredStyled("BACK", tft.width() / 2, 275 + 17, COL_TEXT);
}

void drawReceiptScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();

  bool dispenseFailed = activeTransactionStatus == "FAILED_DISPENSE" ||
                        activeTransactionStatus == "FAILED_CHANGE" ||
                        activeTransactionStatus == "CANCELLED";
  bool partialSuccess = activeTransactionStatus == "PARTIAL_SUCCESS";

  if (partialSuccess) {
    printCenteredStyled("Partial Success", tft.width() / 2, 70, COL_GOLD, true, true);
    int resultY = 115;
    int resultStart = 0;
    while (resultStart < (int)dispenseResultSummary.length()) {
      int resultEnd = dispenseResultSummary.indexOf('\n', resultStart);
      String resultLine = resultEnd == -1
        ? dispenseResultSummary.substring(resultStart)
        : dispenseResultSummary.substring(resultStart, resultEnd);
      printCenteredStyled(resultLine, tft.width() / 2, resultY, COL_TEXT);
      resultY += 20;
      if (resultEnd == -1) break;
      resultStart = resultEnd + 1;
    }
    printCenteredStyled("Check the item results above.", tft.width() / 2, 175, COL_GOLD);
  }
  else if (dispenseFailed) {
    printCenteredStyled("Dispense failed", tft.width() / 2, 85, COL_RED, true, true);
    printCenteredStyled(activeTransactionStatus, tft.width() / 2, 125, COL_TEXT);
    printCenteredStyled("Please contact an administrator.", tft.width() / 2, 150, COL_TEXT);
  }
  else if (activeChangeDueCents == 0) {
    // Scenario 1: Exact payment
    printCenteredStyled("Take your items!", tft.width() / 2, 85, COL_TEXT, true, true);
    printCenteredStyled("Thank you!", tft.width() / 2, 125, COL_GREEN);
  }
  else if (activeChangePaidCents >= activeChangeDueCents && activeChangeDueCents > 0) {
    // Scenario 2: Change successfully released
    printCenteredStyled("Take your items!", tft.width() / 2, 70, COL_TEXT, true, true);
    printCenteredStyled("Change Released:", tft.width() / 2, 105, COL_GREEN);
    printCenteredStyled("PHP " + String(activeChangePaidCents / 100.0, 2),
                        tft.width() / 2, 135, COL_TEXT, false, false, true);
  }
  else {
    // Scenario 3: Change failed or incomplete (Claim message displayed)
    printCenteredStyled("Take your items!", tft.width() / 2, 60, COL_TEXT, true, true);
    printCenteredStyled("Change Owed: P" + String((activeChangeDueCents - activeChangePaidCents) / 100.0, 2),
                        tft.width() / 2, 95, COL_GOLD, false, false, true);
    printCenteredStyled("Please present " + activeTrNumber, tft.width() / 2, 130, COL_RED);
    printCenteredStyled("to the admin to claim.", tft.width() / 2, 150, COL_TEXT);
  }

  // Draw Confirm Button
  tft.fillRoundRect(20, 240, 200, 55, 8, COL_CHECKOUT);
  printCenteredStyled("CONFIRM", tft.width() / 2, 267, COL_TEXT);
}

void drawDispensingScreen() {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();
  printCenteredStyled("Dispensing Items...", tft.width() / 2, 130, COL_TEXT, true, true);
  printCenteredStyled("Please wait for your paper / pens", tft.width() / 2, 165, COL_GOLD);
}

void redrawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_IDLE:        drawIdleScreen();        break;
    case SCREEN_MAIN:        drawMainScreen();        break;
    case SCREEN_PAPER_BRAND: drawPaperBrandScreen();   break;
    case SCREEN_CATALOG:     drawCatalogScreen();     break;
    case SCREEN_CART:        drawCartScreen();        break;
    case SCREEN_SUMMARY:     drawSummaryScreen();     break;
    case SCREEN_DISPENSING:  drawDispensingScreen();  break;
    case SCREEN_RECEIPT:     drawReceiptScreen();     break;
  }
}

void drawStatusScreen(String headline, String message) {
  tft.fillScreen(COL_BACKGROUND);
  drawTftStatusBar();
  printCenteredStyled(headline, tft.width() / 2, 115, COL_TEXT, true, true);
  printCenteredStyled(message, tft.width() / 2, 155, COL_TEXT);
}


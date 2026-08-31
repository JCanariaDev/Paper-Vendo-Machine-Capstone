// TFT DRAW
// Split from revamped_final_mega.ino for readability.

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
  tft.fillRect(0, 0, tft.width(), 26, COL_BLACK);
  tft.setTextSize(1);

  if (activeTrNumber.length() > 0) {
    // Show official TR Record Number on top-left throughout checkout/dispense
    tft.setTextColor(COL_ORANGE);
    tft.setCursor(6, 8);
    tft.print(activeTrNumber);
  } else {
    tft.setTextColor(uiWifiConnected ? COL_GREEN : COL_RED);
    tft.setCursor(6, 8);
    tft.print(uiWifiConnected ? "WIFI OK" : "WIFI --");
  }

  String creditText = "Credits: P" + String((unsigned int)credits);
  int16_t x1, y1; uint16_t w, h;
  tft.getTextBounds(creditText.c_str(), 0, 0, &x1, &y1, &w, &h);
  tft.setTextColor(COL_WHITE);
  tft.setCursor(tft.width() - w - 6, 8);
  tft.print(creditText);
}

void drawIdleScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  if (wifiStatus == WIFI_STATUS_CONNECTED) {
    printCentered("Insert Coins", tft.width() / 2, 140);
    printCentered("to use.", tft.width() / 2, 165);
    return;
  }

  printCentered("Connect to WiFi", tft.width() / 2, 140);
  printCentered("first.", tft.width() / 2, 165);

  tft.setTextSize(1);
  if (wifiStatus == WIFI_STATUS_NOT_FOUND) {
    tft.setTextColor(COL_RED);
    printCentered("WiFi can't be detected", tft.width() / 2, 200);
  }
}

void drawMainScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Select an Option", tft.width() / 2, 48);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  printCentered("BUY PAPER", tft.width() / 2, 122);

  tft.fillRoundRect(20, 160, 200, 55, 8, COL_GREEN);
  printCentered("BUY BALLPEN", tft.width() / 2, 187);

  tft.fillRoundRect(20, 225, 200, 55, 8, COL_DARKGREEN);
  printCentered("CHECKOUT", tft.width() / 2, 252);

  tft.fillRoundRect(20, 285, 200, 30, 8, COL_GREY);
  tft.setTextSize(1);
  String cartLabel = cartCount > 0 ? ("VIEW CART (" + String(cartCount) + ")") : "VIEW CART (empty)";
  printCentered(cartLabel.c_str(), tft.width() / 2, 285 + 15);
}

void drawPaperBrandScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Choose Paper Brand", tft.width() / 2, 55);

  tft.fillRoundRect(20, 95, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  printCentered("BUDGET", tft.width() / 2, 122);
  tft.fillRoundRect(20, 165, 200, 55, 8, COL_GREEN);
  printCentered("STANDARD", tft.width() / 2, 192);
  tft.fillRoundRect(20, 275, 200, 35, 8, COL_GREY);
  tft.setTextSize(1);
  printCentered("BACK", tft.width() / 2, 292);
}

void drawCatalogScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  String title = activeCatalogType == "paper" ? selectedPaperBrand + " Paper" : "Ballpen Options";
  printCentered(title.c_str(), tft.width() / 2, 38);

  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int btnSize = catalogButtonSize(count);
    bool selected = pendingQty[i] > 0;
    bool isAvailable = catalog[i].isPaperPresent;

    if (selected) {
      tft.drawRoundRect(4, rowY, 232, btnSize + 8, 8, COL_GREEN);
      tft.drawRoundRect(5, rowY + 1, 230, btnSize + 6, 8, COL_GREEN);
    }

    // "-" button
    tft.fillRoundRect(8, rowY + 4, btnSize, btnSize, 8, COL_RED);
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(btnSize >= 46 ? 3 : 2);
    printCentered("-", 8 + btnSize / 2, rowY + 4 + btnSize / 2);

    // "+" button
    int plusX = 232 - btnSize;
    tft.fillRoundRect(plusX, rowY + 4, btnSize, btnSize, 8, isAvailable ? COL_GREEN : COL_GREY);
    printCentered("+", plusX + btnSize / 2, rowY + 4 + btnSize / 2);

    // Info Column
    tft.setTextColor(COL_WHITE);
    tft.setTextSize(1);
    tft.setCursor(72, rowY + 6);
    tft.print(catalog[i].name);
    tft.setCursor(72, rowY + 20);
    tft.print("P" + String(catalogDisplayPrice(i), 2));

    if (!isAvailable) {
      tft.setTextColor(COL_RED);
      tft.setCursor(72, rowY + 34);
      tft.print("[OUT OF PAPER]");
    } else {
      char qtyBuf[16];
      sprintf(qtyBuf, "Qty: %d", pendingQty[i]);
      tft.setTextSize(btnSize >= 46 ? 2 : 1);
      tft.setCursor(72, rowY + (btnSize >= 46 ? 34 : 32));
      tft.print(qtyBuf);
    }
  }

  tft.fillRoundRect(15, 275, 95, 35, 8, COL_GREEN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("ADD", 15 + 47, 275 + 17);

  tft.fillRoundRect(130, 275, 95, 35, 8, COL_RED);
  printCentered("CANCEL", 130 + 47, 275 + 17);
}

void drawSummaryScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Order Summary", tft.width() / 2, 40);

  tft.setTextSize(1);
  int y = 70;
  int start = 0;
  while (start < (int)orderSummaryText.length()) {
    int nl = orderSummaryText.indexOf('\n', start);
    String line = (nl == -1) ? orderSummaryText.substring(start) : orderSummaryText.substring(start, nl);
    if (line.length() > 0) {
      tft.setCursor(15, y);
      tft.print(line);
      y += 16;
    }
    if (nl == -1) break;
    start = nl + 1;
  }

  tft.setCursor(15, y + 10);
  tft.print("Total: P" + String(orderTotalCost, 2));

  tft.setTextSize(2);
  if (orderInProgress) {
    tft.setTextColor(COL_ORANGE);
    printCentered("Dispensing...", tft.width() / 2, 258);
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_GREY);
    tft.setTextColor(COL_WHITE);
    printCentered("PLEASE WAIT", tft.width() / 2, 275 + 20);
  } else {
    tft.fillRoundRect(20, 275, 200, 40, 8, COL_GREY);
    tft.setTextColor(COL_WHITE);
    printCentered("ORDER CLOSED", tft.width() / 2, 275 + 20);
  }
}

void drawCartScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("Your Cart", tft.width() / 2, 45);

  tft.setTextSize(1);
  if (cartCount == 0) {
    printCentered("Cart is empty.", tft.width() / 2, 150);
  } else {
    int rowH = cartRowHeight();
    for (int i = 0; i < cartCount; i++) {
      int rowY = cartRowY(i);

      String line = String(cart[i].qty) + "x " + cart[i].name;
      tft.setCursor(10, rowY + 4);
      tft.print(line);
      String priceLine = "P" + String(cart[i].price, 2) + " each - P" + String(cart[i].price * cart[i].qty, 2);
      tft.setCursor(10, rowY + 18);
      tft.print(priceLine);

      int btnSize = min(rowH - 6, 40);
      int btnY = rowY + (rowH - btnSize) / 2;
      tft.fillRoundRect(196, btnY, btnSize, btnSize, 6, COL_RED);
      tft.setTextColor(COL_WHITE);
      tft.setTextSize(btnSize >= 24 ? 2 : 1);
      printCentered("X", 196 + btnSize / 2, btnY + btnSize / 2);
      tft.setTextSize(1);

      if (i < cartCount - 1) {
        tft.drawFastHLine(8, rowY + rowH - 2, 224, COL_GREY);
      }
    }

    tft.setCursor(10, CART_BOTTOM + 6);
    tft.setTextSize(2);
    tft.print("Total: P" + String(cartTotal(), 2));
  }

  tft.fillRoundRect(20, 275, 200, 35, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("BACK", tft.width() / 2, 275 + 17);
}

void drawReceiptScreen() {
  tft.fillScreen(COL_BLACK);
  drawTftStatusBar();

  tft.setTextSize(2);
  if (activeChangeDueCents == 0) {
    // Scenario 1: Exact payment
    tft.setTextColor(COL_WHITE);
    printCentered("Take your items!", tft.width() / 2, 85);
    tft.setTextColor(COL_GREEN);
    printCentered("Thank you!", tft.width() / 2, 125);
  }
  else if (activeChangePaidCents >= activeChangeDueCents && activeChangeDueCents > 0) {
    // Scenario 2: Change successfully released
    tft.setTextColor(COL_WHITE);
    printCentered("Take your items!", tft.width() / 2, 70);
    tft.setTextColor(COL_GREEN);
    printCentered("Change Released:", tft.width() / 2, 105);
    printCentered("PHP " + String(activeChangePaidCents / 100.0, 2), tft.width() / 2, 135);
  }
  else {
    // Scenario 3: Change failed or incomplete (Claim message displayed)
    tft.setTextColor(COL_WHITE);
    printCentered("Take your items!", tft.width() / 2, 60);
    tft.setTextColor(COL_ORANGE);
    printCentered("Change Owed: P" + String((activeChangeDueCents - activeChangePaidCents) / 100.0, 2), tft.width() / 2, 95);
    tft.setTextColor(COL_RED);
    tft.setTextSize(1);
    printCentered("Please present " + activeTrNumber, tft.width() / 2, 130);
    printCentered("to the admin to claim.", tft.width() / 2, 150);
  }

  // Draw Confirm Button
  tft.fillRoundRect(20, 240, 200, 55, 8, COL_BLUE);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered("CONFIRM", tft.width() / 2, 267);
}

void redrawCurrentScreen() {
  switch (currentScreen) {
    case SCREEN_IDLE:        drawIdleScreen();        break;
    case SCREEN_MAIN:        drawMainScreen();        break;
    case SCREEN_PAPER_BRAND: drawPaperBrandScreen();   break;
    case SCREEN_CATALOG:     drawCatalogScreen();     break;
    case SCREEN_CART:        drawCartScreen();        break;
    case SCREEN_SUMMARY:     drawSummaryScreen();     break;
    case SCREEN_RECEIPT:     drawReceiptScreen();     break;
  }
}

void drawStatusScreen(String headline, String message) {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print(headline);

  display.setTextSize(1);
  display.setCursor(0, 48);
  display.print(message);
  display.display();
}


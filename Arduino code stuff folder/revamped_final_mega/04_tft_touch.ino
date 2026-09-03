// TFT TOUCH
// Split from revamped_final_mega.ino for readability.

void handleReceiptTouch(int x, int y) {
  if (x >= 20 && x <= 220 && y >= 240 && y <= 295) {
    activeTrNumber = "";
    activeTransactionId = "";
    activeChangeDueCents = 0;
    activeChangePaidCents = 0;
    currentScreen = SCREEN_IDLE;
    drawIdleScreen();
  }
}

void handleMainTouch(int x, int y) {
  // VIEW CART — works even offline
  if (x >= 20 && x <= 230 && y >= 285 && y <= 315) {
    currentScreen = SCREEN_CART;
    drawCartScreen();
    return;
  }

  if (!uiWifiConnected) {
    tftUiShowError("Connect to WiFi first");
    return;
  }

  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    // BUY PAPER: go directly to the paper catalog.
    activeCatalogType = "paper";
    resetPendingSelections();
    currentScreen = SCREEN_CATALOG;
    drawCatalogScreen();
  } else if (x >= 20 && x <= 220 && y >= 160 && y <= 215) {
    activeCatalogType = "pen";
    resetPendingSelections();
    currentScreen = SCREEN_CATALOG;
    drawCatalogScreen();
  } else if (x >= 20 && x <= 220 && y >= 225 && y <= 280) {
    if (cartCount > 0) {
      startOrder();
    } else {
      tftUiShowError("Cart is empty");
    }
  }
}

void handlePaperBrandTouch(int x, int y) {
  if (x >= 20 && x <= 220 && y >= 95 && y <= 150) {
    selectedPaperBrand = "Budget";
  } else if (x >= 20 && x <= 220 && y >= 165 && y <= 220) {
    selectedPaperBrand = "Standard";
  } else if (x >= 20 && x <= 220 && y >= 275 && y <= 315) {
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  } else {
    return;
  }
  resetPendingSelections();
  currentScreen = SCREEN_CATALOG;
  drawCatalogScreen();
}

void handleCatalogTouch(int x, int y) {
  CatalogItem* catalog = (activeCatalogType == "paper") ? paperCatalog : ballpenCatalog;
  int count = (activeCatalogType == "paper") ? PAPER_COUNT : BALLPEN_COUNT;

  for (int i = 0; i < count; i++) {
    int rowY = catalogRowY(i, count);
    int rowH = catalogRowHeight(count);
    int btnSize = catalogButtonSize(count);
    int btnY = rowY + (rowH - btnSize) / 2;
    int plusX = 232 - btnSize;

    if (x >= 4 && x <= 8 + btnSize + 4 && y >= btnY - 4 && y <= btnY + btnSize + 4) {
      pendingQty[i] = max(0, pendingQty[i] - 1);
      drawCatalogScreen();
      return;
    }
    if (x >= plusX - 4 && x <= plusX + btnSize + 4 && y >= btnY - 4 && y <= btnY + btnSize + 4) {
      if (!catalog[i].isPaperPresent) {
        tftUiShowError(activeCatalogType == "paper" ? "Bay is Out of Paper" : "Out of Stock");
        return;
      }

      float pendingCost = 0;
      for (int j = 0; j < count; j++) pendingCost += pendingQty[j] * catalogDisplayPrice(j);
      float wouldBeCost = cartTotal() + pendingCost + catalogDisplayPrice(i);

      if (wouldBeCost > credits) {
        tftUiShowError("Insufficient credits");
        return;
      }

      pendingQty[i] = min(20, pendingQty[i] + 1);
      drawCatalogScreen();
      return;
    }
  }

  if (x >= 20 && x <= 120 && y >= 270 && y <= 350) { // ADD
    for (int i = 0; i < count; i++) {
      if (pendingQty[i] > 0) {
        addToCart(activeCatalogType, catalog[i].id, catalog[i].name, catalogDisplayPrice(i), pendingQty[i]);
      }
    }
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }

  if (x >= 140 && x <= 240 && y >= 270 && y <= 350) { // CANCEL
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
    return;
  }
}

void handleCartTouch(int x, int y) {
  if (cartCount > 0) {
    int rowH = cartRowHeight();
    for (int i = 0; i < cartCount; i++) {
      int rowY = cartRowY(i);
      int btnSize = min(rowH - 6, 40);
      int btnY = rowY + (rowH - btnSize) / 2;

      if (x >= 192 && x <= 196 + btnSize + 4 && y >= btnY - 4 && y <= btnY + btnSize + 4) {
        removeFromCart(i);
        drawCartScreen();
        return;
      }
    }
  }

  if (x >= 20 && x <= 230 && y >= 320 && y <= 340) { // BACK
    currentScreen = SCREEN_MAIN;
    drawMainScreen();
  }
}

void tftUiLoop() {
  unsigned long now = millis();
  if (now < touchDebounceUntil) return;
  if (!ts.touched()) return;
  touchDebounceUntil = now + 200;

  TS_Point p = ts.getPoint();
  int rawX = p.x, rawY = p.y;
#if TOUCH_SWAP_XY
  int tmp = rawX; rawX = rawY; rawY = tmp;
#endif
  int x = map(rawX, TS_MINX, TS_MAXX, 0, tft.width());
  int y = map(rawY, TS_MINY, TS_MAXY, 0, tft.height());
#if TOUCH_INVERT_X
  x = tft.width() - x;
#endif
#if TOUCH_INVERT_Y
  y = tft.height() - y;
#endif

  Serial.print("Touch at x="); Serial.print(x); Serial.print(" y="); Serial.println(y);

  switch (currentScreen) {
    case SCREEN_MAIN:        handleMainTouch(x, y);        break;
    case SCREEN_PAPER_BRAND: handlePaperBrandTouch(x, y);   break;
    case SCREEN_CATALOG:     handleCatalogTouch(x, y);     break;
    case SCREEN_CART:        handleCartTouch(x, y);        break;
    case SCREEN_RECEIPT:     handleReceiptTouch(x, y);     break;
    default: break;
  }
}


// TFT RUNTIME
// Split from revamped_final_mega.ino for readability.

void tftUiBegin() {
  tft.begin();
  tft.setRotation(2);
  diagTouchOk = ts.begin();
  ts.setRotation(0);
  resetPendingSelections();
  currentScreen = (credits > 0) ? SCREEN_MAIN : SCREEN_IDLE;
  redrawCurrentScreen();
}

void tftUiSetCredits() {
  bool hasCredits = credits >= minimumCreditsToStart;
  if ((currentScreen == SCREEN_IDLE || currentScreen == SCREEN_RECEIPT) && hasCredits) {
    activeTrNumber = "";
    activeTransactionId = "";
    activeTransactionStatus = "";
    activeChangeDueCents = 0;
    activeChangePaidCents = 0;
    currentScreen = SCREEN_MAIN;
    redrawCurrentScreen();
  } else if (currentScreen != SCREEN_IDLE && currentScreen != SCREEN_RECEIPT && !hasCredits && !orderInProgress) {
    currentScreen = SCREEN_IDLE;
    cartCount = 0;
    redrawCurrentScreen();
  } else {
    drawTftStatusBar();
  }
}

void tftUiSetWifiStatus(int status) {
  bool changed = wifiStatus != status;
  wifiStatus = (WifiStatus)status;
  uiWifiConnected = (status == WIFI_STATUS_CONNECTED);
  if (!orderInProgress) refreshMachineAvailability(changed);
  if (status == WIFI_STATUS_CONNECTED && catalogLoaded && !orderInProgress && credits < maximumCreditsAllowed) {
    setCoinAcceptance(true);
  } else if (status != WIFI_STATUS_CONNECTED) {
    setCoinAcceptance(false);
  }
  if (currentScreen == SCREEN_IDLE || currentScreen == SCREEN_MAIN) {
    redrawCurrentScreen();
  } else {
    drawTftStatusBar();
  }
}

void tftUiSetWifiConnected(bool connected) {
  tftUiSetWifiStatus(connected ? WIFI_STATUS_CONNECTED : WIFI_STATUS_IDLE);
}

void drawWifiSpinnerFrame() {
  const int centerX = tft.width() / 2;
  const int centerY = 230;
  const int8_t xOffsets[8] = { 0, 7, 10, 7, 0, -7, -10, -7 };
  const int8_t yOffsets[8] = { -10, -7, 0, 7, 10, 7, 0, -7 };
  const uint16_t trailColors[8] = {
    COL_GOLD, COL_TITLE_EDGE, COL_CREAM, COL_CYAN,
    COL_PEN_BTN, COL_CYAN, COL_CREAM, COL_TITLE_EDGE
  };
  tft.fillCircle(centerX, centerY, 14, COL_BACKGROUND);
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t dot = (spinnerFrame + i) % 8;
    tft.fillCircle(centerX + xOffsets[dot], centerY + yOffsets[dot],
                   i == 0 ? 3 : 2, trailColors[i]);
  }
  spinnerFrame++;
}

void tftUiShowError(String message) {
  uiErrorUntil = millis() + 2500;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_RED);
  printCenteredStyled(message, tft.width() / 2, tft.height() - 24, COL_WHITE);
}

void tftUiShowSuccess(String message) {
  uiSuccessUntil = millis() + 2000;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_PEN_BTN);
  printCenteredStyled(message, tft.width() / 2, tft.height() - 24, COL_TEXT);
}


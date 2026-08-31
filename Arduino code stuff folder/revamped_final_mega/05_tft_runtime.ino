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
  bool hasCredits = credits > 0;
  if ((currentScreen == SCREEN_IDLE || currentScreen == SCREEN_RECEIPT) && hasCredits) {
    activeTrNumber = "";
    activeTransactionId = "";
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
  tft.fillRect(tft.width() / 2 - 10, 190, 20, 20, COL_BLACK);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  char buf[2] = { SPINNER_CHARS[spinnerFrame % 4], '\0' };
  printCentered(buf, tft.width() / 2, 200);
  spinnerFrame++;
}

void tftUiShowError(String message) {
  uiErrorUntil = millis() + 2500;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_RED);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 24);
}

void tftUiShowSuccess(String message) {
  uiSuccessUntil = millis() + 2000;
  tft.fillRect(0, tft.height() - 36, tft.width(), 24, COL_DARKGREEN);
  tft.setTextColor(COL_WHITE);
  tft.setTextSize(2);
  printCentered(message.c_str(), tft.width() / 2, tft.height() - 24);
}


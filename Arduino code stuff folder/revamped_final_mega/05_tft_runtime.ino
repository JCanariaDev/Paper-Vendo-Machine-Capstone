// TFT RUNTIME
// Split from revamped_final_mega.ino for readability.

void tftUiBegin() {
  // Keep both SPI peripherals deselected before either library starts. This
  // prevents the XPT2046 touch controller from driving the shared SPI bus
  // during ILI9341 startup.
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TOUCH_CS, OUTPUT);
  digitalWrite(TOUCH_CS, HIGH);
  pinMode(TFT_DC, OUTPUT);
  pinMode(TFT_RST, OUTPUT);

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
  // A receipt must remain on screen until the customer presses Confirm.
  // Credit/status updates may refresh the balance, but must not dismiss it.
  if (currentScreen == SCREEN_IDLE && hasCredits) {
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
  if (!changed) return;
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


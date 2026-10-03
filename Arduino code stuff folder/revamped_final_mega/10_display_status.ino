// TFT STATUS DISPLAY
// Replaces the former OLED status display.

void updateLCD() {
  // The TFT UI owns the full layout. Refresh the status bar so credit changes
  // remain visible without requiring a second display.
  drawTftStatusBar();
}

void showError(String m) {
  setMachineIndicator(INDICATOR_ERROR, true);
  tftUiShowError(m);
  if (orderInProgress) {
    orderInProgress = false;
    setCoinAcceptance(true);
    cartCount = 0;
    currentScreen = SCREEN_MAIN;
  }
  delay(2000);
  refreshMachineAvailability();
  isProcessing = false;
  updateLCD();
  redrawCurrentScreen();
}

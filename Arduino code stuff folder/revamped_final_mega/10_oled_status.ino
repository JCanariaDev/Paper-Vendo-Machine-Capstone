// OLED STATUS
// Split from revamped_final_mega.ino for readability.

void updateLCD() {
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Smart Vendo V3");

  display.setTextSize(2);
  display.setCursor(0, 18);
  display.print("P");
  display.print((int)credits);

  display.setTextSize(1);
  display.setCursor(0, 52);
  display.print("Ready to Serve");
  display.display();
}

void showError(String m) {
  setMachineIndicator(INDICATOR_ERROR, true);
  drawStatusScreen("Error", m);
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


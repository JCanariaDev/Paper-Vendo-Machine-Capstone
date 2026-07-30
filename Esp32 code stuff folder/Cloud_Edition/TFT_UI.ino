#define USER_SETUP_LOADED
#define ILI9341_DRIVER
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST   4
#define TOUCH_CS 21
#define SPI_FREQUENCY 10000000

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

uint16_t calData[5] = {328, 3531, 336, 3434, 7};

enum TftScreen {
  TFT_SCREEN_IDLE,
  TFT_SCREEN_MAIN_MENU,
  TFT_SCREEN_PAPER_MENU,
  TFT_SCREEN_BALLPEN_MENU,
  TFT_SCREEN_ORDER_SUMMARY,
  TFT_SCREEN_PROCESSING,
  TFT_SCREEN_SUCCESS,
  TFT_SCREEN_ERROR
};

TftScreen currentTftScreen = TFT_SCREEN_IDLE;
int tftCredits = 0;
bool tftWifiConnected = false;
bool tftNeedsRedraw = true;
String tftStatusMessage = "";
unsigned long tftMessageStartedAt = 0;

struct UiButton {
  int x;
  int y;
  int w;
  int h;
  String label;
};

void tftDrawButton(UiButton button, uint16_t fillColor, uint16_t textColor);
bool tftIsInside(uint16_t touchX, uint16_t touchY, UiButton button);
void tftHandleMainMenuTouch(uint16_t touchX, uint16_t touchY);
void tftHandleProductMenuTouch(uint16_t touchX, uint16_t touchY);
void tftUiShowProductMenu(const char* title, const char* options[], int optionCount);
void tftDrawProductRow(int rowIndex, const char* label, bool selected, int qty);
void tftSetProductSelection(int index);
void tftHandleSummaryTouch(uint16_t touchX, uint16_t touchY);
void tftUiShowOrderSummary();
void tftUiShowMessage(const char* title, String message, uint16_t color);
void tftUiSetWifiConnected(bool connected);
void submitTftOrder(String type, int id, int qty);

UiButton paperButton = {25, 90, 190, 46, "Paper"};
UiButton ballpenButton = {25, 150, 190, 46, "Ballpen"};
UiButton completeButton = {25, 230, 190, 46, "Complete"};
UiButton addButton = {15, 282, 95, 30, "Add"};
UiButton cancelButton = {130, 282, 95, 30, "Cancel"};
UiButton dispenseButton = {15, 252, 95, 42, "Dispense"};
UiButton backButton = {130, 252, 95, 42, "Back"};

const char* paperOptions[] = {
  "Budget 1/4",
  "Budget Cross",
  "Budget Length",
  "Budget Whole",
  "Std 1/4",
  "Std Cross",
  "Std Length",
  "Std Whole"
};

const char* ballpenOptions[] = {
  "Budget Pen",
  "Standard Pen"
};

int selectedOptionIndex = -1;
int selectedQuantity = 1;
String cartType = "";
String cartName = "";
int cartId = 0;
int cartQuantity = 0;

void tftUiBegin() {
  tft.init();
  tft.setRotation(0);
  tft.setTouch(calData);
  tft.fillScreen(TFT_BLACK);
  tftUiShowIdle();
}

void tftUiLoop() {
  if (tftNeedsRedraw) {
    tftUiRedraw();
  }

  if ((currentTftScreen == TFT_SCREEN_SUCCESS || currentTftScreen == TFT_SCREEN_ERROR) &&
      millis() - tftMessageStartedAt > 3000) {
    currentTftScreen = (tftCredits > 0) ? TFT_SCREEN_MAIN_MENU : TFT_SCREEN_IDLE;
    tftNeedsRedraw = true;
    return;
  }

  uint16_t touchX = 0;
  uint16_t touchY = 0;
  if (!tft.getTouch(&touchX, &touchY)) return;

  delay(160); // Basic debounce while UI is still being scaffolded.

  if (currentTftScreen == TFT_SCREEN_MAIN_MENU) {
    tftHandleMainMenuTouch(touchX, touchY);
  } else if (currentTftScreen == TFT_SCREEN_PAPER_MENU ||
             currentTftScreen == TFT_SCREEN_BALLPEN_MENU) {
    tftHandleProductMenuTouch(touchX, touchY);
  } else if (currentTftScreen == TFT_SCREEN_ORDER_SUMMARY) {
    tftHandleSummaryTouch(touchX, touchY);
  }
}

void tftUiSetCredits(int credits) {
  if (credits == tftCredits) return;

  tftCredits = credits;
  if (currentTftScreen == TFT_SCREEN_PROCESSING ||
      currentTftScreen == TFT_SCREEN_SUCCESS ||
      currentTftScreen == TFT_SCREEN_ERROR) {
    return;
  }

  currentTftScreen = (tftWifiConnected && tftCredits > 0) ? TFT_SCREEN_MAIN_MENU : TFT_SCREEN_IDLE;
  tftNeedsRedraw = true;
}

void tftUiSetWifiConnected(bool connected) {
  if (connected == tftWifiConnected) return;

  tftWifiConnected = connected;
  if (!connected) {
    // Do not allow cloud-dependent item selection while offline.
    currentTftScreen = TFT_SCREEN_IDLE;
  } else if (tftCredits > 0) {
    currentTftScreen = TFT_SCREEN_MAIN_MENU;
  } else {
    currentTftScreen = TFT_SCREEN_IDLE;
  }
  tftNeedsRedraw = true;
}

void tftUiRedraw() {
  if (currentTftScreen == TFT_SCREEN_IDLE) {
    tftUiShowIdle();
  } else if (currentTftScreen == TFT_SCREEN_MAIN_MENU) {
    tftUiShowMainMenu();
  } else if (currentTftScreen == TFT_SCREEN_PAPER_MENU) {
    tftUiShowProductMenu("Paper", paperOptions, 8);
  } else if (currentTftScreen == TFT_SCREEN_BALLPEN_MENU) {
    tftUiShowProductMenu("Ballpen", ballpenOptions, 2);
  } else if (currentTftScreen == TFT_SCREEN_ORDER_SUMMARY) {
    tftUiShowOrderSummary();
  } else if (currentTftScreen == TFT_SCREEN_PROCESSING) {
    tftUiShowMessage("Processing", "Checking order...", TFT_ORANGE);
  } else if (currentTftScreen == TFT_SCREEN_SUCCESS) {
    tftUiShowMessage("Success", tftStatusMessage, TFT_DARKGREEN);
  } else if (currentTftScreen == TFT_SCREEN_ERROR) {
    tftUiShowMessage("Error", tftStatusMessage, TFT_RED);
  }

  tftNeedsRedraw = false;
}

void tftUiShowIdle() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  if (!tftWifiConnected) {
    tft.setTextSize(2);
    tft.drawString("Connect to a", 120, 135);
    tft.drawString("Wifi first!", 120, 165);
  } else {
    tft.drawString("Insert Coins", 120, 145);
    tft.drawString("to Use", 120, 175);
  }
}

void tftUiShowMainMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Paper and Ballpen", 120, 12);
  tft.drawString("Vending Machine", 120, 28);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.drawString("P" + String(tftCredits), 15, 52);

  tftDrawButton(paperButton, TFT_BLUE, TFT_WHITE);
  tftDrawButton(ballpenButton, TFT_DARKGREEN, TFT_WHITE);
  tftDrawButton(completeButton, TFT_ORANGE, TFT_BLACK);
}

void tftDrawButton(UiButton button, uint16_t fillColor, uint16_t textColor) {
  tft.fillRoundRect(button.x, button.y, button.w, button.h, 6, fillColor);
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 6, TFT_WHITE);
  tft.setTextColor(textColor, fillColor);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString(button.label, button.x + button.w / 2, button.y + button.h / 2);
}

bool tftIsInside(uint16_t touchX, uint16_t touchY, UiButton button) {
  return touchX >= button.x &&
         touchX <= button.x + button.w &&
         touchY >= button.y &&
         touchY <= button.y + button.h;
}

void tftHandleMainMenuTouch(uint16_t touchX, uint16_t touchY) {
  if (tftIsInside(touchX, touchY, paperButton)) {
    Serial.println("TFT: Paper button tapped");
    selectedOptionIndex = -1;
    selectedQuantity = 1;
    currentTftScreen = TFT_SCREEN_PAPER_MENU;
    tftNeedsRedraw = true;
  } else if (tftIsInside(touchX, touchY, ballpenButton)) {
    Serial.println("TFT: Ballpen button tapped");
    selectedOptionIndex = -1;
    selectedQuantity = 1;
    currentTftScreen = TFT_SCREEN_BALLPEN_MENU;
    tftNeedsRedraw = true;
  } else if (tftIsInside(touchX, touchY, completeButton)) {
    Serial.println("TFT: Complete transaction tapped");
    if (cartQuantity > 0) {
      currentTftScreen = TFT_SCREEN_ORDER_SUMMARY;
      tftNeedsRedraw = true;
    } else {
      Serial.println("TFT: Complete ignored, no item added");
    }
  }
}

void tftUiShowProductMenu(const char* title, const char* options[], int optionCount) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString(title, 120, 8);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("Select item and quantity", 12, 34);

  for (int i = 0; i < optionCount; i++) {
    int rowQty = (selectedOptionIndex == i) ? selectedQuantity : 1;
    tftDrawProductRow(i, options[i], selectedOptionIndex == i, rowQty);
  }

  tftDrawButton(addButton, TFT_DARKGREEN, TFT_WHITE);
  tftDrawButton(cancelButton, TFT_MAROON, TFT_WHITE);
}

void tftDrawProductRow(int rowIndex, const char* label, bool selected, int qty) {
  int y = 52 + rowIndex * 26;
  uint16_t rowColor = selected ? TFT_NAVY : TFT_BLACK;
  uint16_t borderColor = selected ? TFT_CYAN : TFT_DARKGREY;

  tft.fillRect(4, y, 232, 24, rowColor);
  tft.drawRect(4, y, 232, 24, borderColor);

  tft.fillRect(8, y + 3, 20, 18, TFT_DARKGREY);
  tft.fillRect(56, y + 3, 20, 18, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("-", 18, y + 12);
  tft.drawString("+", 66, y + 12);

  tft.setTextColor(TFT_WHITE, rowColor);
  tft.setTextSize(1);
  tft.drawString(String(qty), 42, y + 12);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(label, 84, y + 12);

  tft.drawRect(210, y + 5, 14, 14, TFT_WHITE);
  if (selected) {
    tft.fillRect(213, y + 8, 8, 8, TFT_CYAN);
  }
}

void tftHandleProductMenuTouch(uint16_t touchX, uint16_t touchY) {
  if (tftIsInside(touchX, touchY, cancelButton)) {
    selectedOptionIndex = -1;
    selectedQuantity = 1;
    currentTftScreen = TFT_SCREEN_MAIN_MENU;
    tftNeedsRedraw = true;
    Serial.println("TFT: Product menu cancelled");
    return;
  }

  if (tftIsInside(touchX, touchY, addButton)) {
    if (selectedOptionIndex < 0) {
      Serial.println("TFT: Add ignored, no item selected");
      return;
    }

    const char** options = (currentTftScreen == TFT_SCREEN_PAPER_MENU) ? paperOptions : ballpenOptions;
    cartType = (currentTftScreen == TFT_SCREEN_PAPER_MENU) ? "paper" : "pen";
    cartId = selectedOptionIndex + 1;
    cartName = options[selectedOptionIndex];
    cartQuantity = selectedQuantity;

    Serial.println("TFT: Added " + cartType + " " + String(cartId) + " x" + String(cartQuantity) + " - " + cartName);
    currentTftScreen = TFT_SCREEN_MAIN_MENU;
    tftNeedsRedraw = true;
    return;
  }

  int optionCount = (currentTftScreen == TFT_SCREEN_PAPER_MENU) ? 8 : 2;
  for (int i = 0; i < optionCount; i++) {
    int y = 52 + i * 26;
    if (touchY < y || touchY > y + 24) continue;

    if (touchX >= 8 && touchX <= 28) {
      tftSetProductSelection(i);
      if (selectedQuantity > 1) selectedQuantity--;
    } else if (touchX >= 56 && touchX <= 76) {
      tftSetProductSelection(i);
      if (selectedQuantity < 99) selectedQuantity++;
    } else if (touchX >= 84 && touchX <= 226) {
      tftSetProductSelection(i);
    }

    tftNeedsRedraw = true;
    return;
  }
}

void tftUiShowOrderSummary() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Order Summary", 120, 10);

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("Item:", 15, 52);
  tft.drawString(cartName, 15, 70);
  tft.drawString("Type: " + cartType, 15, 98);
  tft.drawString("Qty: " + String(cartQuantity), 15, 116);
  tft.drawString("Credits: P" + String(tftCredits), 15, 134);
  tft.drawString("Price checked on dispense", 15, 164);

  tftDrawButton(dispenseButton, TFT_DARKGREEN, TFT_WHITE);
  tftDrawButton(backButton, TFT_MAROON, TFT_WHITE);
}

void tftHandleSummaryTouch(uint16_t touchX, uint16_t touchY) {
  if (tftIsInside(touchX, touchY, backButton)) {
    currentTftScreen = TFT_SCREEN_MAIN_MENU;
    tftNeedsRedraw = true;
    Serial.println("TFT: Summary back tapped");
    return;
  }

  if (tftIsInside(touchX, touchY, dispenseButton)) {
    currentTftScreen = TFT_SCREEN_PROCESSING;
    tftNeedsRedraw = true;
    Serial.println("TFT: Dispense tapped");
    submitTftOrder(cartType, cartId, cartQuantity);
  }
}

void tftUiShowProcessing() {
  currentTftScreen = TFT_SCREEN_PROCESSING;
  tftNeedsRedraw = true;
}

void tftUiShowSuccess(String message) {
  tftStatusMessage = message;
  tftMessageStartedAt = millis();
  cartType = "";
  cartName = "";
  cartId = 0;
  cartQuantity = 0;
  currentTftScreen = TFT_SCREEN_SUCCESS;
  tftNeedsRedraw = true;
  Serial.println("TFT success: " + message);
}

void tftUiShowError(String message) {
  tftStatusMessage = message;
  tftMessageStartedAt = millis();
  currentTftScreen = TFT_SCREEN_ERROR;
  tftNeedsRedraw = true;
  Serial.println("TFT error: " + message);
}

void tftUiShowMessage(const char* title, String message, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.setTextColor(color, TFT_BLACK);
  tft.drawString(title, 120, 120);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(message, 120, 155);
}

void tftSetProductSelection(int index) {
  if (selectedOptionIndex != index) {
    selectedOptionIndex = index;
    selectedQuantity = 1;
  }
}

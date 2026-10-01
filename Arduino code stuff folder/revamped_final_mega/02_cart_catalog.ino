// CART CATALOG
// Split from revamped_final_mega.ino for readability.

float cartTotal() {
  float sum = 0;
  for (int i = 0; i < cartCount; i++) sum += cart[i].price * cart[i].qty;
  return sum;
}

float catalogDisplayPrice(int index) {
  // Price is dynamically set from ESP32 catalog sync per bay
  if (activeCatalogType == "paper") {
    return paperCatalog[index].price;
  }
  return ballpenCatalog[index].price;
}

void resetPendingSelections() {
  for (int i = 0; i < MAX_CATALOG_ROWS; i++) {
    pendingQty[i] = 0;
  }
}

int ballpenCartQuantity() {
  int total = 0;
  for (int i = 0; i < cartCount; i++) {
    if (cart[i].type == "pen") total += cart[i].qty;
  }
  return total;
}

int ballpenPendingQuantity() {
  if (activeCatalogType != "pen") return 0;
  int total = 0;
  for (int i = 0; i < BALLPEN_COUNT; i++) total += pendingQty[i];
  return total;
}

void removeFromCart(int index) {
  if (index < 0 || index >= cartCount) return;
  for (int i = index; i < cartCount - 1; i++) {
    cart[i] = cart[i + 1];
  }
  cartCount--;
}

void addToCart(String type, int id, const char* name, float price, int qty) {
  if (type == "pen" && ballpenCartQuantity() + qty > maximumBallpensPerTransaction) {
    return;
  }
  for (int i = 0; i < cartCount; i++) {
    if (cart[i].type == type && cart[i].id == id) {
      cart[i].qty += qty;
      return;
    }
  }
  if (cartCount < MAX_CART_ITEMS) {
    cart[cartCount].type = type;
    cart[cartCount].id = id;
    cart[cartCount].name = String(name);
    cart[cartCount].price = price;
    cart[cartCount].qty = qty;
    cartCount++;
  }
}

void startSerialBallpenOrder(int quantity) {
  if (quantity < 1) {
    Serial.println("Usage: PEN <quantity> (example: PEN 1)");
    return;
  }
  if (orderInProgress || currentScreen == SCREEN_RECEIPT || cartCount > 0) {
    Serial.println("Cannot start serial order: finish/confirm the current screen and clear the cart first.");
    return;
  }
  if (!uiWifiConnected) {
    Serial.println("Cannot start serial order: ESP32 is not reporting Wi-Fi connected.");
    return;
  }
  if (quantity < minimumBallpensPerTransaction ||
      quantity > maximumBallpensPerTransaction) {
    Serial.print("Ballpen quantity must be ");
    Serial.print(minimumBallpensPerTransaction);
    Serial.print("-");
    Serial.println(maximumBallpensPerTransaction);
    return;
  }
  if (BALLPEN_COUNT < 1 || ballpenCatalog[0].id <= 0 ||
      !ballpenCatalog[0].isPaperPresent || ballpenCatalogStock[0] < quantity) {
    Serial.println("Cannot start serial order: ballpen bay is empty or catalog stock is not synced.");
    return;
  }

  const float total = ballpenCatalog[0].price * quantity;
  if (credits < minimumCreditsToStart || total > credits) {
    Serial.print("Insufficient inserted credits. Need P");
    Serial.print(total, 2);
    Serial.print("; available P");
    Serial.println(credits);
    return;
  }

  addToCart("pen", ballpenCatalog[0].id, ballpenCatalog[0].name,
            ballpenCatalog[0].price, quantity);
  Serial.print("Starting recorded ballpen checkout: ");
  Serial.print(quantity);
  Serial.print(" x ");
  Serial.print(ballpenCatalog[0].name);
  Serial.print(" for P");
  Serial.println(total, 2);
  startOrder();
}


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
  if (type == "pen" && ballpenCartQuantity() + qty > MAX_BALLPENS_PER_TRANSACTION) {
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


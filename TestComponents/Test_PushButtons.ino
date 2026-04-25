/* 
  Test_PushButtons_LCD_Transaction.ino
  - Pin 30: PAPER Transaction
  - Pin 38: PEN Transaction
  - Includes LCD Output (Pins 20, 21)
*/
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // To ESP32
  
  pinMode(30, INPUT_PULLUP);
  pinMode(38, INPUT_PULLUP);
  
  lcd.init();
  lcd.backlight();
  updateLCD("Ready to Serve", "Press Button");
  
  Serial.println("--- 2-BUTTON LCD TRANSACTION TEST ---");
}

void loop() {
  // Check Pin 30 (Paper)
  if (digitalRead(30) == LOW) {
    Serial.println("\n[1] Sending REQ for PAPER (ID 1, P10.0)");
    updateLCD("Checking Cloud..", "Paper Request");
    Serial1.println("REQ:paper:1:10.0");
    delay(1000); // Debounce
  } 

  // Check Pin 38 (Pen)
  if (digitalRead(38) == LOW) {
    Serial.println("\n[1] Sending REQ for BALLPEN (ID 1, P10.0)");
    updateLCD("Checking Cloud..", "Pen Request");
    Serial1.println("REQ:pen:1:10.0");
    delay(1000); // Debounce
  }

  // Listen for ESP32 response
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    
    if (msg.startsWith("DISPENSE:")) {
      // Parse DISPENSE:QTY:COST:NAME
      int f1 = msg.indexOf(':');
      int f2 = msg.indexOf(':', f1 + 1);
      int f3 = msg.indexOf(':', f2 + 1);
      
      String qty = msg.substring(f1 + 1, f2);
      String cost = msg.substring(f2 + 1, f3);
      String name = msg.substring(f3 + 1);
      String type = (qty.toInt() > 1) ? "paper" : "pen";

      updateLCD("AUTHORIZED!", name);
      delay(2000); 
      
      updateLCD("Dispensing...", name);
      delay(2000); // Simulate motor
      
      // Send DONE to log in Supabase
      updateLCD("Logging Cloud...", "Please wait");
      Serial1.println("DONE:" + type + ":1:" + name + ":" + cost + ":" + qty);
      
      updateLCD("Success! Done", "Take Item");
      delay(3000);
      updateLCD("Ready to Serve", "Press Button");
    } 
    else if (msg.startsWith("ERR:")) {
      updateLCD("Cloud Error", msg.substring(4));
      delay(3000);
      updateLCD("Ready to Serve", "Press Button");
    }
  }
}

void updateLCD(String L1, String L2) {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print(L1);
  lcd.setCursor(0,1); lcd.print(L2);
}

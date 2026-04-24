/* 
  Test_Keypad_Transaction.ino
  - Button '6': Paper Transaction
  - Button 'B': Ballpen Transaction
*/
#include <Keypad.h>

const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {22, 23, 24, 25};
byte colPins[COLS] = {27, 26, 28, 29}; 

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // Communication with ESP32
  
  Serial.println("--- KEYPAD CLOUD TRANSACTION TEST ---");
  Serial.println("Press '6' for PAPER | Press 'B' for BALLPEN");
}

void loop() {
  char key = keypad.getKey();
  
  if (key) {
    if (key == '6') {
      Serial.println("\n[1] Sending REQ for PAPER (ID 1, P10.0)");
      Serial1.println("REQ:paper:1:10.0");
    } 
    else if (key == 'B') {
      Serial.println("\n[1] Sending REQ for BALLPEN (ID 1, P10.0)");
      Serial1.println("REQ:pen:1:10.0");
    }
  }

  // Listen for ESP32 response
  if (Serial1.available()) {
    String msg = Serial1.readStringUntil('\n');
    msg.trim();
    
    if (msg.startsWith("DISPENSE:")) {
      Serial.print("[2] AUTHORIZED: "); Serial.println(msg);
      
      // Parse DISPENSE:QTY:COST:NAME
      int f1 = msg.indexOf(':');
      int f2 = msg.indexOf(':', f1 + 1);
      int f3 = msg.indexOf(':', f2 + 1);
      
      String qty = msg.substring(f1 + 1, f2);
      String cost = msg.substring(f2 + 1, f3);
      String name = msg.substring(f3 + 1);
      String type = (qty.toInt() > 1) ? "paper" : "pen";

      Serial.println("[3] Dispensing " + qty + " units of " + name);
      delay(2000); // Simulate motor movement
      
      // Send DONE to log in Supabase
      Serial.println("[4] Logging to Cloud...");
      Serial1.println("DONE:" + type + ":1:" + name + ":" + cost + ":" + qty);
      Serial.println(">>> TRANSACTION COMPLETE!");
    } 
    else if (msg.startsWith("ERR:")) {
      Serial.print("!!! ERROR FROM CLOUD: "); Serial.println(msg);
    }
  }
}

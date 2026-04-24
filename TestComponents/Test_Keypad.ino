/* 
  Test_Keypad.ino
  WIRING GUIDE:
  - Keypad Pin 1 (Leftmost) -> Arduino Mega Pin 22 (R1)
  - Keypad Pin 2           -> Arduino Mega Pin 23 (R2)
  - Keypad Pin 3           -> Arduino Mega Pin 24 (R3)
  - Keypad Pin 4           -> Arduino Mega Pin 25 (R4)
  - Keypad Pin 5           -> Arduino Mega Pin 26 (C1)
  - Keypad Pin 6           -> Arduino Mega Pin 27 (C2)
  - Keypad Pin 7           -> Arduino Mega Pin 28 (C3)
  - Keypad Pin 8 (Right)   -> Arduino Mega Pin 29 (C4)
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
byte colPins[COLS] = {26, 27, 28, 29};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void setup() {
  Serial.begin(115200);
  Serial.println("--- KEYPAD TEST ---");
  Serial.println("Press any key on the keypad.");
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key Pressed: ");
    Serial.println(key);
  }
}

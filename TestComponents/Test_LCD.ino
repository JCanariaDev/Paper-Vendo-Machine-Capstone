/* 
  Test_LCD.ino
  WIRING GUIDE (I2C):
  - LCD GND -> Arduino Mega GND
  - LCD VCC -> Arduino Mega 5V
  - LCD SDA -> Arduino Mega Pin 20
  - LCD SCL -> Arduino Mega Pin 21
*/
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("LCD TEST OK!");
  lcd.setCursor(0,1);
  lcd.print("VENDO ONLINE");
}

void loop() {
  // Blink cursor to show life
  lcd.cursor();
  delay(500);
  lcd.noCursor();
  delay(500);
}

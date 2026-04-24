/* 
  Test_LCD.ino
  WIRING GUIDE (I2C):
  - LCD GND -> Arduino Mega GND
  - LCD VCC -> Arduino Mega 5V
  - LCD SDA -> Arduino Mega Pin 20
  - LCD SCL -> Arduino Mega Pin 21
*/
#include <LiquidCrystal_I2C.h>

// Set the LCD address to 0x27 or 0x3F for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);
  Serial.println("--- LCD DEBUG START ---");
  
  Serial.println("1. Initializing LCD...");
  lcd.init();
  
  Serial.println("2. Turning on Backlight...");
  lcd.backlight();
  
  Serial.println("3. Printing text to screen...");
  lcd.setCursor(0,0);
  lcd.print("LCD TEST OK!");
  lcd.setCursor(0,1);
  lcd.print("VENDO ONLINE");
  
  Serial.println("--- INITIALIZATION DONE ---");
  Serial.println("If you don't see text, turn the BLUE SCREW on the back!");
}

void loop() {
  // Blink cursor to show life
  lcd.cursor();
  delay(500);
  lcd.noCursor();
  delay(500);
}

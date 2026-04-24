/* 
  Test_LoadCell.ino
  WIRING GUIDE (HX711 Module):
  - HX711 VCC -> Arduino Mega 5V
  - HX711 GND -> Arduino Mega GND
  - HX711 DT  -> Arduino Mega Pin 5
  - HX711 SCK -> Arduino Mega Pin 6
*/
#include <HX711.h>

HX711 scale;

void setup() {
  Serial.begin(115200);
  scale.begin(5, 6);
  Serial.println("--- LOAD CELL TEST ---");
  Serial.println("Tare... Remove everything from scale.");
  scale.tare();
  Serial.println("Ready. Put something on the scale.");
}

void loop() {
  if (scale.is_ready()) {
    long reading = scale.get_units(10);
    Serial.print("Weight Reading (Raw): ");
    Serial.println(reading);
  }
  delay(500);
}

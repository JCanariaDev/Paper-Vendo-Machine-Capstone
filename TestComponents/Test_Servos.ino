/* 
  Test_Servos.ino
  WIRING GUIDE:
  - Servo 1 Signal (Orange) -> Arduino Mega Pin 9
  - Servo 2 Signal (Orange) -> Arduino Mega Pin 10
  - Both Red Wires          -> Arduino Mega 5V
  - Both Brown Wires        -> Arduino Mega GND
*/
#include <Servo.h>

Servo s9, s10;

void setup() {
  Serial.begin(115200);
  s9.attach(9);
  s10.attach(10);
  Serial.println("--- SERVO TEST ---");
}

void loop() {
  Serial.println("Moving to 90 degrees...");
  s9.write(90);
  s10.write(90);
  delay(1000);

  Serial.println("Moving back to 0 degrees...");
  s9.write(0);
  s10.write(0);
  delay(1000);
}

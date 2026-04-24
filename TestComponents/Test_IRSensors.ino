/* 
  Test_IRSensors.ino
  WIRING GUIDE:
  - Sensor 1 OUT -> Arduino Mega Pin 7
  - Sensor 2 OUT -> Arduino Mega Pin 8
  - Both VCC     -> Arduino Mega 5V
  - Both GND     -> Arduino Mega GND
*/

void setup() {
  Serial.begin(115200);
  pinMode(7, INPUT);
  pinMode(8, INPUT);
  Serial.println("--- IR SENSOR TEST ---");
}

void loop() {
  bool s1 = digitalRead(7);
  bool s2 = digitalRead(8);

  Serial.print("Sensor 1: "); Serial.print(s1 ? "CLEAN" : "BLOCKED");
  Serial.print(" | Sensor 2: "); Serial.println(s2 ? "CLEAN" : "BLOCKED");
  
  delay(200);
}

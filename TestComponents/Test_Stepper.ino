/* 
  Test_Stepper_Performance.ino
  WIRING:
  - IN1 -> Pin 3
  - IN2 -> Pin 4
  - IN3 -> Pin 11
  - IN4 -> Pin 12
*/

#include <Stepper.h>

// 2048 is the full revolution for this geared motor
const int stepsPerRevolution = 2048; 

// The sequence 1-3-2-4 (3, 11, 4, 12) is the standard for ULN2003
Stepper myStepper(stepsPerRevolution, 3, 11, 4, 12);

void setup() {
  // 15 is the "Sweet Spot" for this motor. 
  // 20 might be too fast and cause it to jam.
  myStepper.setSpeed(15); 
  
  Serial.begin(115200);
  Serial.println("--- PERFORMANCE STEPPER TEST ---");
}

void loop() {
  Serial.println(">>> Moving 180 Degrees Fast...");
  myStepper.step(stepsPerRevolution / 2); 
  
  // Turn off pins to keep motor cool
  stopMotor();
  delay(2000);

  Serial.println(">>> Moving Back 180 Degrees Fast...");
  myStepper.step(-stepsPerRevolution / 2); 
  
  stopMotor();
  delay(2000);
}

void stopMotor() {
  digitalWrite(3, LOW);
  digitalWrite(4, LOW);
  digitalWrite(11, LOW);
  digitalWrite(12, LOW);
}

/* 
  Test_LoadCell_Calibration.ino
  WIRING: DT=5, SCK=6
  
  HOW TO CALIBRATE:
  1. Open Serial Monitor (115200).
  2. Put your Phone (195g) on the scale.
  
  USE THESE KEYS TO MATCH 195g:
  - MAKE WEIGHT BIGGER:  q(+1), a(+10), s(+100), d(+1000), f(+10000)
  - MAKE WEIGHT SMALLER: w(-1), z(-10), x(-100), c(-1000), v(-10000)
*/
#include <HX711.h>

HX711 scale;
float calibration_factor = 420.0; 

void setup() {
  Serial.begin(115200);
  scale.begin(5, 6);
  
  Serial.println("--- LOAD CELL CALIBRATION TOOL ---");
  Serial.println("1. Remove all weight from scale...");
  delay(2000);
  scale.set_scale();
  scale.tare(); 
  
  Serial.println("2. Put your Phone (195g) on the scale.");
  Serial.println("USE KEYS TO MATCH 195.0g:");
  Serial.println("BIGGER: q(+1), a(+10), s(+100), d(+1000), f(+10000)");
  Serial.println("SMALLER: w(-1), z(-10), x(-100), c(-1000), v(-10000)");
}

void loop() {
  scale.set_scale(calibration_factor);

  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1);
  Serial.print(" g");
  Serial.print("  |  Factor: ");
  Serial.println(calibration_factor);

  if(Serial.available()) {
    char temp = Serial.read();
    // Swapped logic so 'top row' always increases the WEIGHT reading
    if(temp == 'q') calibration_factor -= 1;
    else if(temp == 'w') calibration_factor += 1;
    else if(temp == 'a') calibration_factor -= 10;
    else if(temp == 'z') calibration_factor += 10;
    else if(temp == 's') calibration_factor -= 100;
    else if(temp == 'x') calibration_factor += 100;
    else if(temp == 'd') calibration_factor -= 1000;
    else if(temp == 'c') calibration_factor += 1000;
    else if(temp == 'f') calibration_factor -= 10000;
    else if(temp == 'v') calibration_factor += 10000;
  }
}

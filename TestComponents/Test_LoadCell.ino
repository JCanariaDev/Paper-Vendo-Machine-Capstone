/* 
  Test_LoadCell_Calibration.ino
  WIRING: DT=5, SCK=6
  
  HOW TO CALIBRATE:
  1. Open Serial Monitor (115200).
  2. Put a known weight on the scale (e.g. 100g).
  3. Use 'a', 's', 'd', 'f' to increase calibration factor.
  4. Use 'z', 'x', 'c', 'v' to decrease calibration factor.
  5. Once the weight matches your object, COPY the 'Final Factor' 
     into your vendo_machine.ino code!
*/
#include <HX711.h>

HX711 scale;
float calibration_factor = 420.0; // Start with this

void setup() {
  Serial.begin(115200);
  scale.begin(5, 6);
  
  Serial.println("--- LOAD CELL CALIBRATION TOOL ---");
  Serial.println("1. Remove all weight from scale...");
  delay(2000);
  scale.set_scale();
  scale.tare(); // Reset to 0
  
  Serial.println("2. Put a KNOWN weight on the scale.");
  Serial.println("Use keyboard to adjust factor:");
  Serial.println("Increase: a(+10), s(+100), d(+1000), f(+10000)");
  Serial.println("Decrease: z(-10), x(-100), c(-1000), v(-10000)");
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
    if(temp == 'a') calibration_factor += 10;
    else if(temp == 's') calibration_factor += 100;
    else if(temp == 'd') calibration_factor += 1000;
    else if(temp == 'f') calibration_factor += 10000;
    else if(temp == 'z') calibration_factor -= 10;
    else if(temp == 'x') calibration_factor -= 100;
    else if(temp == 'c') calibration_factor -= 1000;
    else if(temp == 'v') calibration_factor -= 10000;
  }
}

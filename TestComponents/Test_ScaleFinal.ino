/* 
  Test_ScaleFinal.ino
  Use this to test your accuracy and estimate sheets.
*/
#include <HX711.h>

HX711 scale;
const float CALIBRATION_FACTOR = 730.0; // Your calibrated number
const float WEIGHT_PER_SHEET = 2.5;    // Approx weight of one 1/4 sheet (Adjust this!)

void setup() {
  Serial.begin(115200);
  scale.begin(5, 6);
  scale.set_scale(CALIBRATION_FACTOR);
  
  Serial.println("--- FINAL SCALE TEST ---");
  Serial.println("Taring... Keep scale empty.");
  delay(2000);
  scale.tare();
  Serial.println("Ready!");
}

void loop() {
  float currentWeight = scale.get_units(10); // Average of 10 readings for stability
  
  if (currentWeight < 0) currentWeight = 0; // Ignore tiny negative numbers
  
  int estimatedSheets = currentWeight / WEIGHT_PER_SHEET;

  Serial.print("Weight: ");
  Serial.print(currentWeight, 1);
  Serial.print(" g  |  Estimated Sheets: ~");
  Serial.println(estimatedSheets);

  delay(500);
}

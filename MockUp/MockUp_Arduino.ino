/*
  MockUp_Arduino.ino
  Description: Simple test for one compartment (Ballpen).
  Connect a button between Pin 2 and GND.
*/

const int buttonPin = 2; // The button for "Buy Ballpen"

void setup() {
  Serial.begin(9600); // Communicate with ESP32 at 9600 baud
  pinMode(buttonPin, INPUT_PULLUP); // Use internal pullup, button connects Pin 2 to GND
  
  Serial.println("Arduino Ready! Press the button to buy a ballpen.");
}

void loop() {
  // Check if button is pressed (LOW because of INPUT_PULLUP)
  if (digitalRead(buttonPin) == LOW) {
    delay(50); // Debounce
    if (digitalRead(buttonPin) == LOW) {
      
      // Send a simulated "DONE" message to the ESP32
      // Format: DONE:TYPE:ID:NAME:PRICE:QTY
      // We simulate buying 1 Standard Ballpen (ID 2 in our SQL) for 10 Pesos
      Serial.println("DONE:pen:2:Standard Ballpen:10.0:1");
      
      Serial.println("Arduino Sent: SOLD! Waiting for next press...");
      
      // Wait for release
      while(digitalRead(buttonPin) == LOW); 
      delay(500); 
    }
  }
}

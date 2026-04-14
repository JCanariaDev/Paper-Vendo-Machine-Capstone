const int buttonPin = 2;

void setup() {
  Serial.begin(9600);  // This talks to your LAPTOP
  Serial1.begin(9600); // This talks to the ESP32 (Pin 18)
  
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Arduino Mega Ready! Using TX1 (Pin 18) to talk to ESP32.");
}

void loop() {
  if (digitalRead(buttonPin) == LOW) {
    delay(50); 
    if (digitalRead(buttonPin) == LOW) {
      
      // We use Serial1 to send the message to the ESP32!
      Serial1.println("DONE:pen:2:Standard Ballpen:10.0:1");
      
      // We use Serial to show YOU what happened on the laptop
      Serial.println(">>> Button Pressed! Sending signal to ESP32 over Pin 18...");
      
      while(digitalRead(buttonPin) == LOW); 
      delay(500); 
    }
  }
}

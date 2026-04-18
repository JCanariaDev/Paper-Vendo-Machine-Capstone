const int buttonPin = 2;

void setup() {
  Serial.begin(9600);  // This talks to your LAPTOP
  Serial1.begin(9600); // This talks to the ESP32 (Pin 18)
  
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.println("Arduino Mega Ready! Using TX1 (Pin 18) to talk to ESP32.");
}

void loop() {
  // 1. Check for physical button press
  if (digitalRead(buttonPin) == LOW) {
    delay(50); 
    if (digitalRead(buttonPin) == LOW) {
      Serial.println(">>> Button Pressed! Asking Cloud for Price & Sheets...");
      
      // We send a REQUEST. We simulate that we have 10 Pesos (Credits)
      // Format: REQ:TYPE:ID:COINS
      Serial1.println("REQ:paper:1:10.0");
      
      while(digitalRead(buttonPin) == LOW); 
      delay(500); 
    }
  }

  // 2. Listen for the Cloud's "GO" signal
  if (Serial1.available()) {
    String resp = Serial1.readStringUntil('\n');
    resp.trim();
    
    if (resp.startsWith("DISPENSE:")) {
      Serial.print(">>> CLOUD APPROVED: "); Serial.println(resp);
      
      // Parse the Cloud's orders (Format: DISPENSE:SHEETS:COST:NAME)
      int first = resp.indexOf(':');
      int second = resp.indexOf(':', first + 1);
      int third = resp.indexOf(':', second + 1);
      
      String sheets = resp.substring(first + 1, second);
      String cost = resp.substring(second + 1, third);
      String name = resp.substring(third + 1);
      
      Serial.println(">>> Action: Spinning motors " + sheets + " times...");
      delay(2000); // Simulate motor movement
      
      // 3. Send final "DONE" report to ESP32 for logging
      Serial1.println("DONE:paper:1:" + name + ":" + cost + ":" + sheets);
      Serial.println(">>> SUCCESS! Order reported to Cloud.");
    } 
    else if (resp.startsWith("ERR:")) {
      Serial.print(">>> CLOUD REJECTED: "); Serial.println(resp);
    }
  }
}

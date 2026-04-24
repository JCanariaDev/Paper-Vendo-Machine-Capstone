/* 
  Test_CoinAcceptor.ino
  WIRING GUIDE:
  - Coin Acceptor Red Wire   -> External 12V Positive (+)
  - Coin Acceptor White Wire -> Arduino Mega Pin 2 (Signal)
  - Coin Acceptor Black Wire -> Arduino Mega GND (Common Ground)
*/

volatile int pulseCount = 0;

void setup() {
  Serial.begin(115200);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), coinPulse, FALLING);
  Serial.println("--- COIN TEST ---");
  Serial.println("Insert a coin and watch the count below.");
}

void coinPulse() {
  pulseCount++;
}

void loop() {
  static int lastCount = -1;
  if (pulseCount != lastCount) {
    Serial.print("Coins/Pulses Detected: ");
    Serial.println(pulseCount);
    lastCount = pulseCount;
  }
}

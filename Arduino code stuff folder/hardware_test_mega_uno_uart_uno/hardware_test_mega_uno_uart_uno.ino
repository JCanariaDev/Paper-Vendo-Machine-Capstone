/*
  UNO SIDE OF THE MEGA -> UNO UART TEST

  Same main-system UART pins:
    Uno D0 / RX <- Mega D16 / TX2
    Uno D1 / TX -> Mega D17 / RX2
    Uno GND     -> Mega GND
    UART speed  = 9600 baud

  The Uno USB serial monitor shares D0/D1. Disconnect the Mega UART wires
  while uploading this sketch, then reconnect them before testing.
*/

const unsigned long UART_BAUD_RATE = 9600;

void setup() {
  Serial.begin(UART_BAUD_RATE);
  Serial.setTimeout(100);
  delay(300);
  Serial.println(F("Uno UART receiver ready. Waiting for SIGNAL..."));
}

void loop() {
  if (Serial.available() == 0) return;

  String message = Serial.readStringUntil('\n');
  message.trim();

  if (message == "SIGNAL") {
    Serial.println(F("Signal received"));
    Serial.println(F("ACK_SIGNAL"));
  }
}

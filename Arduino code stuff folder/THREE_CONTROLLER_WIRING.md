# Three-Controller Wiring

## Arduino Mega

- `Serial1` D18/D19: ESP32 gateway
- `Serial2` D16/D17: Paper Uno
- `Serial3` D14/D15: Ballpen Uno
- D22: coin hopper relay IN
- D23: coin hopper exit IR OUT
- TFT remains on D47-D53
- Coin acceptor pulse remains on D2

The previous hopper connections on D14/D15 must be moved to D22/D23 because
D14/D15 are now the Ballpen Uno UART pins.

## Ballpen Uno

- D0 RX: from Mega D14 TX3
- D1 TX: to Mega D15 RX3
- GND: common with Mega GND
- ULN2003 IN1/IN2/IN3/IN4: D3/D4/D11/D12
- Ballpen IR OUT: D7, active LOW
- Green LED: D8
- Red LED: D9
- Passive buzzer: D10
- Blue LED: D13

Power the ULN2003 motor from its suitable external 5V supply. Connect that
supply GND to the Ballpen Uno GND and the Mega GND.

## Paper Uno

The existing Paper Uno wiring and protocol remain unchanged:

- D0/D1: Mega Serial2
- Bay 1 NEMA17: STEP D2, DIR D3
- Bay 2 NEMA17: STEP D4, DIR D5
- Driver ENABLE: D10
- Exit IR sensors: D11/D12
- Level IR sensors: D6/D7

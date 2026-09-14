# Mega and Uno Ballpen Isolation Test

## Mega to ballpen Uno UART

| Mega | Ballpen Uno | Purpose |
|---|---|---|
| D14 / TX3 | D0 / RX | Mega sends commands |
| D15 / RX3 | D1 / TX | Uno sends results |
| GND | GND | Common reference |

Use 9600 baud. Do not connect 12V to any UART pin.

This uses the Mega's `Serial3`, so it does not share the ESP32 UART
(`Serial1`, D18/D19) or the paper Uno UART (`Serial2`, D16/D17).

## Mega test controller

The Mega test sketch uses only the TFT and touchscreen:

- TFT CS: D53
- TFT DC: D48
- TFT RESET: D49
- Touch CS: D47
- SPI MISO/MOSI/SCK: D50/D51/D52

The Mega test sketch does not use the ULN2003, ballpen IR, buzzer, or LEDs.

## Ballpen Uno

- ULN2003 IN1/IN2/IN3/IN4: D3/D4/D11/D12
- Ballpen IR OUT: D7, active LOW
- Green LED: D8
- Red LED: D9
- Passive buzzer: D10
- Blue LED: D13

Power the ULN2003 motor from a suitable external 5V supply and connect that
supply ground to Uno GND. Do not power the motor through an Arduino GPIO pin.

Disconnect D0/D1 from the Mega while uploading the Uno sketch, then reconnect
them before running the test.

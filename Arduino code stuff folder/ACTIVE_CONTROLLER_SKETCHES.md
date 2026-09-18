# Active controller sketches

Use these three folders as the current three-controller firmware set:

| Controller | Folder and sketch | Mega communication |
| --- | --- | --- |
| Master controller | `revamped_final_mega/revamped_final_mega.ino` | — |
| Paper controller | `revamped_uno_paper/revamped_uno_paper.ino` | Serial2 at 9600: Mega D16/TX2 -> Uno D0/RX; Mega D17/RX2 <- Uno D1/TX |
| Ballpen controller | `revamped_uno_ballpen/revamped_uno_ballpen.ino` | Serial3 at 9600: Mega D14/TX3 -> Uno D0/RX; Mega D15/RX3 <- Uno D1/TX |

Connect Mega GND, Paper Uno GND, and Ballpen Uno GND together. The UART lines are crossed: TX connects to RX and RX connects to TX.

The Mega source in `revamped_final_mega` already contains the three-controller integration: paper commands use `UNO_SERIAL` (Serial2), ballpen commands use `BALLPEN_SERIAL` (Serial3), and the hopper is assigned to Mega D22/D23. The previous versions remain preserved in the `backup_three_controller_before_ballpen_uno_2026-09-14` folder.

Before uploading either Uno sketch over USB, disconnect its D0/D1 wires to the Mega if the Arduino IDE cannot upload reliably. Reconnect them afterward.

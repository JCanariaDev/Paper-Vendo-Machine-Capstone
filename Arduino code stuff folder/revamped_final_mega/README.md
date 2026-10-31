# Revamped Mega Firmware Notes

## Live Serial Order and Hopper Test

Use the Mega Serial Monitor at 115200 baud with a newline line ending. Wait for
Wi-Fi and catalog sync, then insert real coins until the displayed/serial
credit balance is greater than the selected product price. With the TFT
connected but not being used, enter a live order preview such as:

- `SERIAL_ORDER PAPER 1 1` to preview one item from paper Bay 1.
- `SERIAL_ORDER PEN 1 1` to preview one pen from Bay 1.
- `SERIAL_ORDER CONFIRM` to submit the preview as a real ESP32/cloud order.
- `SERIAL_ORDER CANCEL` to discard the preview.

The preview expires after 30 seconds. Confirmation reserves the current real
credits and may charge them, dispense the selected product, and release change.
This is not an unbilled test. To exercise the hopper, ensure the paid amount is
greater than the actual cloud price so the returned plan includes change. The
Mega Serial Monitor reports the reservation, `CHANGE STAGE`, hopper relay
state, D23 sensor edges/count, timeout or completion, and cloud finalization.
The hopper motor supply must be connected for this live test; keep hands clear
of the mechanism and secure the coin outlet. Do not use `HOPPER ON` for this
test because it bypasses the sensor-counted transaction flow.

The current hopper sensor input is Mega D23, active LOW (LOW=beam blocked,
HIGH=clear). For a sensor-only bench test, keep hopper motor power disconnected
and power the sensor module from a regulated 5V peripheral rail. Connect its
OUT to D23 and its GND to Mega GND. Do not connect 12V to the sensor VCC or join
the sensor buck's +5V output to the Mega's USB-powered +5V rail.

## Paper Hardware Test Command

From the Mega Serial Monitor (115200 baud, newline line ending), enter:

- `PAPER_TEST 1 3` to feed 3 sheets from Bay 1.
- `PAPER_TEST 2 3` to feed 3 sheets from Bay 2.

The command uses the Mega-to-Paper-Uno UART and prints each `SHEET_OK` response
and the final confirmed count. It is an unbilled hardware test: it does not
create a cloud transaction or charge credits. Run only while no touchscreen
transaction or coin burst is active. Physical sheets dispensed during a test
must be accounted for in inventory separately.

## Revision 2026.10.02.1

Adds a compact Mega-to-ESP32 order trace for checkout troubleshooting.

- The Mega records `CHECKOUT sent`, `ESP32 ack`, and `PLAN received` with `millis()` timestamps.
- A checkout watchdog flushes the trace with `WATCHDOG checkout timeout` and sends a classified timeout reason to the ESP32.
- A successful finish flushes it with `FINISHED received`.
- The ESP32 stores each received trace in `machine_logs` as source `MEGA`, event type `MEGA_TRACE`, level `INFO`.
- A checkout watchdog event is stored as `CHECKOUT_WATCHDOG_TIMEOUT`; checkout network, database, or malformed-request failures are stored as `CHECKOUT_FAILED` (or `CHECKOUT_REJECTED` for the configured ballpen limit).
- Trace timestamps are milliseconds since the Mega booted. The database `created_at` is when the log row was inserted, not the time represented by each trace marker.

Interpretation:

- `CHECKOUT sent` followed by `WATCHDOG`, without `ESP32 ack`: Machine Logs should show `CHECKOUT_WATCHDOG_TIMEOUT` with `NO_ESP32_ACK`. Check the Mega/ESP32 UART link, ESP32 loop responsiveness, and power/reset events.
- `CHECKOUT sent`, `ESP32 ack`, then `WATCHDOG`: Machine Logs should show `ACK_RECEIVED_NO_PLAN`. Check nearby `CHECKOUT_FAILED` and `ESP32_RESET` events and the database/backend response.
- No `MEGA_TRACE` row is not conclusive by itself: the ESP32 may have been offline, reset, or unable to persist the event. Check the Mega serial output and nearby reset/network logs too.

After uploading both the Mega and ESP32 sketches, place a test order and inspect Machine Logs for the matching `MEGA_TRACE`, `CHECKOUT_FAILED`, and `ESP32_RESET` entries. Capture the rows and send them with their messages and event times for diagnosis.

## Revision 2026.10.02.2

Controller-health polling now pauses during active orders so Uno dispense time is not mistaken for a disconnect. Paper Uno and Ballpen Uno connection events are logged only when their observed state changes. The coin-pulse handling and checkout trace changes from revision `2026.10.02.1` remain included.

## Revision 2026.10.02.3

The Mega sends each checkout trace checkpoint immediately to the ESP32 and still emits a combined `TIMELINE` on finish or checkout watchdog. The ESP32 can persist a useful trace even if the Mega resets before the transaction completes.

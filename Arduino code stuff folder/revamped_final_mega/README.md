# Revamped Mega Firmware Notes

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

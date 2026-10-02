# Revamped ESP32 Gateway Notes

## Machine monitor status publishing - 2026-10-02

- ESP32 revision `2026.10.02.7` upserts `machine_online_status` on each heartbeat instead of PATCHing only an existing row. This recreates the `id=1` status row if it was deleted, allowing the webapp's heartbeat-based online indicator to recover. Heartbeats are scheduled immediately after Wi-Fi connects.
- The backend status endpoint now surfaces errors reading `machine_online_status` instead of silently reporting the machine Offline when the heartbeat query itself failed.
- No SQL changes are required for this firmware update.

## Checkout responsiveness update - 2026-10-02

- ESP32 revision `2026.10.02.6` clears a deferred credit-session write only after the checkout RPC succeeds. This prevents an already-consumed credit amount from being replayed as a new `CREDIT_HELD` TR after the order finishes; failed checkout attempts retain the pending credit for recovery. A `CREDIT_SESSION_RECONCILED` log records when a positive pending write was superseded. Pending `current_credits` status updates are now allowed while a customer has positive credits, so the Machine Monitor no longer remains at zero during an active credit session.
- Mega UART traffic gets a 200 ms quiet window before queued HTTPS work or finish retries can start. Checkout acknowledgement and dispense-plan delivery are recorded as `CHECKOUT_ACK_SENT` and `CHECKOUT_PLAN_SENT`; each reply is flushed to UART before network work begins. Coin credit receipt and successful database persistence are visible as `CREDIT_UPDATE_RECEIVED` and `CREDIT_SESSION_SAVED` events.
- Mega UART lines are drained before background work, with a bounded serial line timeout.
- System-event and hardware-event logging are queued instead of running inside the UART handler; each write is short-bounded and retried with backoff. Paper-bay updates, status/catalog synchronization, and remote Wi-Fi configuration polling wait until the customer session is idle.
- Only one noncritical database task runs at a time, and none starts while Mega UART input is waiting.
- The machine-online heartbeat remains enabled during customer sessions; it yields whenever Mega UART input is already waiting.
- Catalog and Wi-Fi-configuration HTTP requests have shorter time limits so stale backend requests are less likely to block a later order.
- Startup status/catalog/config work is deferred until the main loop is servicing UART; remote Wi-Fi config polling waits until the machine has been idle for 30 seconds.
- Mega checkout checkpoints arrive as individual `DBG:` messages and are queued as `MEGA_TRACE` rows; trace and error events are protected from eviction by noncritical diagnostics, and successful database writes are confirmed in the ESP32 Serial Monitor.
- Machine Logs displays each Mega trace row as a readable checkout timeline. The page requests the maximum supported 1,000 recent log events.
- No SQL changes are required for this firmware update.

Upload `revamped_final_esp32.ino` from this folder to the ESP32. The Mega's active sketch remains `revamped_final_mega.ino` in the Mega folder; its controller-health transition logging is revision `2026.10.02.2`.

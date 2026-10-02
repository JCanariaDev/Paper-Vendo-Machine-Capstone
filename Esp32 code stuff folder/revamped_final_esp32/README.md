# Revamped ESP32 Gateway Notes

## Checkout responsiveness update - 2026-10-02

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

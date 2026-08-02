  Cloud_remake6.ino - Gateway for Paper Vendo Machine
  Revision history has moved to README.md - see that file for the full
  changelog. This header just tracks the current version:

  Revision 6 - 31/07/2026: Merged two divergent ESP32 sketches.
  KEPT from the newer "production_esp32" file (the right architecture
  for this machine, not being replaced):
    - RESERVE/RESERVED/PLAN/FINISH protocol talking to Supabase RPC
      functions (machine_reserve_transaction, machine_mark_change_paid,
      machine_finish_transaction) instead of doing credit-vs-price math
      locally on the ESP32 - the database enforces the reservation
      atomically, which the old file's client-side check couldn't do.
    - Per-line physical_channel from getTransactionPlan() - required
      for the 4-channel paper dispenser; the old protocol had no way to
      express which of the 4 feeders to use.
    - CHANGE_OK/CHANGE_FAIL handshake matching the Mega's verified
      change-before-dispense logic.
    - STATUS? on-demand query from the Mega.

  PORTED BACK IN from the older "Cloud_remake5" file (present there,
  missing here - this is what was actually broken):
    - The WiFi.mode(WIFI_OFF) -> delay(100) -> WIFI_STA sequence. This
      was the fix for a real, reproduced "assert failed:
      xEventGroupSetBits" crash on first boot - see README.md rev 4 for
      the full bisection story. The newer file jumped straight to
      WIFI_STA with no debug output, so if it hit the same crash it
      would fail completely silently.
    - WiFi.setSleep(false) / setAutoReconnect(true) / persistent(false).
    - A network scan before connecting, and WIFISTATE:CONNECTING /
      WIFISTATE:NOTFOUND messages to the Mega, so it can show a spinner
      vs. "WiFi can't be detected" instead of one generic state -
      pointless without this, since the newer file never sent them.
    - Verbose Serial debug output through the whole connect sequence.
    - The non-blocking "reconnect after 20s stuck" watchdog in loop().
    - updateMachineStatus()/updateStatusKey() - posts is_running and
      WiFi signal strength to Supabase every 60s. The newer file
      dropped this entirely; if a webapp/admin dashboard reads that
      table, it would have gone silently dark.

  ALSO FIXED: SUPABASE_ANON_KEY was split across 3 lines in the source
  file without closing each line's string literal (a raw newline inside
  an unescaped string is a compile error in standard C++) - reassembled
  as one single-line string.
# Revision History

## Mega (renew*.ino) — Master Controller

**Revision 2**
Full touchscreen UI flow (idle -> main menu -> catalog selection with
checkboxes/qty steppers -> cart -> order summary -> dispense change),
replacing the old 3-button ITEM1/ITEM2/CONFIRM demo.

**Revision 5**
Added 2 more stepper motors (3 total) so each of the 3 ballpen catalog
slots has its own dedicated dispenser + IR sensor, instead of sharing a
single motor.

**Revision 6 — 29/07/2026**
Added onboard diagnostics. Runs automatically once at startup and can
be re-run any time by typing DIAG into the Serial Monitor (USB serial,
separate from the CLOUD_SERIAL line to the ESP32). Type DIAG:MOVE1 /
DIAG:MOVE2 / DIAG:MOVE3 to jog a specific pen stepper a small amount as
a physical sanity check.

**Revision 7 — 30/07/2026**
Removed the TFT.readcommand8() SPI readback from diagnostics. On this
clone ILI9341 board, sending that read command was leaving the display
unable to receive further draw calls after boot - which looked like
"TFT frozen on idle screen, never updates after a coin insert" even
though credits/OLED/touch all kept working fine. Diagnostics now just
prompts a visual check instead.

**Revision 8 — 31/07/2026**
Idle/main screens now show "Connect to WiFi first" when the ESP32
hasn't reported a connection yet, and catalog access is blocked with
the same message until it does. Also added a Serial debug print
whenever a "WIFI:" status message actually arrives from the ESP32 over
CLOUD_SERIAL - if that line never prints, the Mega isn't receiving
anything from the ESP32 at all, which points to the physical wiring
between the Mega's Serial1 pins (18 TX1 / 19 RX1) and the ESP32's
Serial2 pins (17 TX2 / 16 RX2) rather than a code issue - double check
those are connected and crossed correctly (Mega TX1 -> ESP32 RX2, Mega
RX1 <- ESP32 TX2).

**Revision 9 — 31/07/2026**
Catalog screen's +/- buttons and checkboxes are small (30x40 / 24x24px)
next to the main screen's huge buttons (200x55px) - the same touch
imprecision that never mattered on the big buttons was likely causing
frequent misses on the small ones. Widened the tappable hit zones
around each +/-/checkbox with a forgiveness margin (visual size
unchanged). Also added a raw touch coordinate debug print. Also added a
"VIEW CART" button on the main screen (works even without WiFi, since
it's just showing what's already added) and a new cart screen listing
items/quantities/prices.

**Revision 10 — 31/07/2026**
Widening the hit zones in rev 9 wasn't enough - reported symptom was
that tapping the checkbox kicked back to the main screen entirely,
meaning taps were landing on the ADD/CANCEL buttons below it (a bigger
calibration mismatch than a small margin fixes). Removed the checkbox
entirely - an item now counts as selected simply by having qty > 0,
shown via a green row border instead of a separate checkbox tap. +/-
buttons are now much larger (full row height instead of 40px), removing
a whole class of small, easy-to-miss targets rather than just padding
around them.

**Revision 11 — 31/07/2026**
The Serial touch-coordinate debug print from rev 9 isn't practical to
read while physically testing the touchscreen on the machine itself
(can't watch a tethered laptop and tap the screen at the same time).
Added the same coordinates directly onto the TFT's own bottom strip
after every touch instead - no laptop needed to read it. Remove this
once touch is confirmed reliable, since it's a temporary diagnostic
aid, not part of the normal UI.

**Revision 12 — 31/07/2026**
On-screen touch readout confirmed a real X-axis mirror - tapping the
"+" button (right side of screen, x172-236) was reporting x=25-63 (the
"-" button's zone, left side). Y-axis was landing close to the correct
row each time, so only X needed correcting. Flipped TOUCH_INVERT_X from
0 to 1.

**Revision 13 — 31/07/2026**
Added a 4th paper option (PAPER_COUNT 3->4). The catalog screen used a
fixed 70px row height sized for exactly 3 rows - adding a 4th would
have overflowed the screen. Row height and button size are now computed
dynamically from how many items are in the active catalog, so ballpen
(3 items) still gets big buttons and paper (4 items) automatically gets
slightly more compact ones to fit - no separate layouts or scrolling
needed. This is a layout change, not a touch recalibration - the
TOUCH_INVERT_X/Y fix from rev 12 is unrelated to item count and still
applies as-is.

  renew15.ino - Master Controller for Paper & Pen Vendo
  Revision history has moved to README.md - see that file for the full
  changelog. This header just tracks the current version:
  Revision 15 - 31/07/2026: The "+" button on the catalog screen now
  checks whether adding one more of that item would exceed inserted
  credits (cart total + everything pending in the current catalog view
  + the new item) - blocks the tap and shows "Insufficient credits"
  instead of letting quantity climb past what was actually paid. Also
  shifted the error/success banners up slightly so they don't get
  instantly painted over by the bottom touch-debug readout.

  renew16.ino - Master Controller for Paper & Pen Vendo
  Revision history has moved to README.md - see that file for the full
  changelog. This header just tracks the current version:
  Revision 16 - 31/07/2026: Cart screen now has a per-item "X" remove
  button, so a mistaken item can be taken back out before checkout.
  Row layout (and the remove button's tappable zone) is computed by
  shared cartRowY()/cartRowHeight() helpers used by both the drawing
  and touch-handling code, same pattern used to fix the catalog screen's
  earlier drift bugs - so this can't fall out of sync the same way.
---

## ESP32 (Cloud_remake*.ino) — Network Gateway

**Revision 3**
Touchscreen moved to the Mega (see mega_1.ino). This sketch is now a
pure network gateway - TFT_UI.ino is no longer needed and should be
removed from this sketch folder.

**Revision 4 — 30/07/2026**
connectToWifi() was crashing on the very first boot with "assert
failed: xEventGroupSetBits" the instant WiFi.mode(WIFI_STA) ran - a
known ESP32 Arduino-core issue where the WiFi driver's internal event
group isn't cleanly settled before the very first mode change. Added an
explicit WiFi.mode(WIFI_OFF) + short delay before that first WIFI_STA
call to give the driver a clean, deliberate first transition instead of
jumping straight to WIFI_STA.
/* STI2D-OS V2 — double π shortcut.
 *
 * A pure detector: the two home screens (STI2D-OS and STI2D Classic) feed it
 * with the π key's press edges taken from their own key loop, so it never
 * scans the keyboard itself and no other screen is affected:
 *   - one π press does nothing here (π keeps its meaning everywhere else:
 *     in the lists it is the letter j of the search, in Matrices it is π);
 *   - two π presses within STI_DOUBLE_PI_WINDOW_MS switch the interface;
 *   - a third press starts a new pair (no triple trigger).
 * π is eadk_key_pi (27) in EADK's keyboard state. */
#include "sti2d_v2.h"

static uint32_t first_ms;
static bool armed;

void sti_v2_pi_reset(void) { armed = false; }

bool sti_v2_pi_feed(bool pi_pressed_edge, uint32_t now_ms) {
  if (armed && now_ms - first_ms > STI_DOUBLE_PI_WINDOW_MS) armed = false;
  if (!pi_pressed_edge) return false;
  if (armed) {
    armed = false;
    return true;
  }
  armed = true;
  first_ms = now_ms;
  return false;
}

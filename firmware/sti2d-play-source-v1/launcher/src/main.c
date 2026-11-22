/* STI2D-Play + STI2D-OS V2 layer
 * The V1 launcher with the V2 layer (firmware/v2): preferences, PIN gate, and
 * sti_home() opening the chosen interface (STI2D-OS or STI2D Classic).
 */
#include <eadk.h>
#include "np.h"
#include "sys.h"
#include "sti2d.h"
#include "sti2d_v2.h"
#include "ui.h"

#if STI_USERLAND
/* (see sti_ul_run below) */
#elif PLATFORM_DEVICE && !NP_SIMULATOR
const uint32_t eadk_api_level __attribute__((section(".rodata.eadk_api_level"))) = 0;
const uint8_t numplay_launcher[1] = {1};

static bool home_pressed;
eadk_keyboard_state_t np_keyboard_scan(void) {
  eadk_keyboard_state_t s = eadk_keyboard_scan();
  if (s & ((1ull << eadk_key_home) | (1ull << eadk_key_on_off))) home_pressed = true;
  return s;
}
eadk_event_t np_event_get(int32_t *timeout) {
  eadk_event_t e = eadk_event_get(timeout);
  if (e == 6 || e == 8) home_pressed = true;
  return e;
}
#else
static bool home_pressed;
#endif

#if STI_USERLAND
/* ------------------------------------------------------------------------
 * STI2D-OS userland: no main(). The Epsilon userland calls sti_ul_run() every
 * time its home screen is shown (apps/sti2d/host.cpp), with the external apps
 * RAM as arena. The shell returns what the user opened: an Epsilon app, an
 * installed app, or the Epsilon grid. */
#include "sti_userland.h"

bool sti_ul_pin_enabled(void) { return sti_v2_pin_enabled(); }

void sti_ul_run(uint8_t *arena, uint32_t size, bool lock, bool returning, sti_ul_request_t *out) {
  np_ul_arena = arena;
  np_ul_arena_size = size;
  np_alloc_reset();
  out->kind = STI_UL_NONE;
  out->index = 0;
  out->entry = 0;
  np_finish_pending_uninstalls();
  sti_v2_load();
  sti_load();
  ui_init();
  /* the lock screen cannot be left without the PIN (or the recovery code) */
  if (lock)
    while (sti_v2_pin_unlock() != 0) ui_init();
  np_wait_release();
  for (;;) {
    int r = sti_home(returning);
    if (r == STI_EXTERNAL && sti_external) {
      out->kind = STI_UL_EXTERNAL;
      out->entry = sti_external;
      break;
    }
    if (r <= STI_BUILTIN_BASE) {
      out->kind = STI_UL_BUILTIN;
      out->index = STI_BUILTIN_BASE - r;
      break;
    }
    if (r == STI_GRID) {
      out->kind = STI_UL_GRID;
      break;
    }
    /* STI_QUIT (Back, Home): this is the home screen, there is nothing to quit to */
    np_wait_release();
    returning = true;
  }
  np_wait_release();
}
#else
int main(void) {
  for (;;) {
    np_session_begin();
    np_finish_pending_uninstalls();
    np_progress_restore();
    np_progress_backup();

    /* STI2D-OS V2: preferences, then the theme (shared with STI2D Classic), then the PIN gate.
     * The gate draws with the launcher's renderer, so the buffers are set up first. */
    sti_v2_load();
    sti_load();
    ui_init();
    if (sti_v2_pin_unlock() != 0) return np_session_end();
    np_wait_release();
    bool returning = false;
    int r;
    for (;;) {
      r = sti_home(returning);
      if (r < 0) break;
      home_pressed = false;
      np_wait_release();
      np_game_run(r);
      np_progress_backup();
      if (home_pressed) return np_session_end();
      np_wait_release();
      returning = true;
    }
    if (r != STI_EXTERNAL) return np_session_end();
    void (*volatile entry)(void) = sti_external;
    np_session_end();
    entry();
    sti_reinit_ram();
  }
}
#endif

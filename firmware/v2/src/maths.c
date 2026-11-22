/* STI2D-OS V2 — "Calculatrice": the NumWorks apps (Calculs, Grapheur,
 * Python...) from the STI2D-OS home screen.
 *
 * Only in the userland build: there the STI2D-OS shell is the calculator's
 * home screen, and the official apps of the Epsilon userland it runs in are
 * opened from here (the host gives their names, in the calculator's language,
 * and opens the one chosen). The last row opens Epsilon's own grid of icons.
 * In the app build (.nwa) the calculator's home screen is Epsilon's, so this
 * screen does not exist. */
#include <string.h>
#include "sti_v2_ui.h"

#if STI_USERLAND
#include "sti_userland.h"

#define ROW_H 24
#define ROW_STEP 27
#define VISIBLE 7
#define MAX_ROWS 24

typedef struct {
  int row, top, n; /* n: builtin apps, then the grid row */
} maths_t;

static void maths_scene(void *ctx) {
  maths_t *s = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  char right[8];
  v2_put_uint(right, (uint32_t)s->n);
  v2_header("Calculatrice", right);
  int total = s->n + 1;
  for (int i = 0; i < VISIBLE; i++) {
    int r = s->top + i;
    if (r >= total) break;
    int y = V2_HEAD_H + 6 + i * ROW_STEP;
    if (r < s->n) {
      bool ok = sti_host_builtin_allowed(r);
      v2_row(y, ROW_H, r == s->row, sti_host_builtin_name(r), ok ? ">" : "Examen", false);
    } else {
      v2_row(y, ROW_H, r == s->row, "Grille NumWorks", "toutes les apps  >", false);
    }
  }
  if (total > VISIBLE) {
    int h = VISIBLE * ROW_STEP - 3, bh = h * VISIBLE / total;
    int by = V2_HEAD_H + 6 + (h - bh) * s->top / (total - VISIBLE);
    gfx_rrect(SCREEN_W - 6, V2_HEAD_H + 6, 3, h, 1, p->panel, 256);
    gfx_rrect(SCREEN_W - 6, by, 3, bh, 1, p->dim, 256);
  }
  v2_footer("OK ouvrir   1-9 acces direct   Back retour");
}

int sti_v2_maths(void) {
  static maths_t s;
  int n = sti_host_builtin_count();
  if (n > MAX_ROWS) n = MAX_ROWS;
  memset(&s, 0, sizeof s); /* from the top, as Epsilon's lists */
  s.n = n;
  int total = n + 1;
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (p & V2_KB(eadk_key_up)) s.row = (s.row + total - 1) % total, dirty = true;
    if (p & V2_KB(eadk_key_down)) s.row = (s.row + 1) % total, dirty = true;
    int d = v2_digit(p);
    bool open = v2_is_ok(p);
    if (d >= 1 && d <= total) s.row = d - 1, open = true;
    if (open) {
      if (s.row >= n) return STI_GRID;
      if (sti_host_builtin_allowed(s.row)) return STI_BUILTIN_BASE - s.row;
      v2_message("Mode examen", "Cette application est desactivee par le mode examen.", NULL);
      v2_keys_init(&k);
      dirty = true;
    }
    if (s.row < s.top) s.top = s.row;
    if (s.row >= s.top + VISIBLE) s.top = s.row - VISIBLE + 1;
    if (dirty) {
      ui_frame(maths_scene, &s);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}
#endif

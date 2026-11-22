/* STI2D-OS V2 — PIN screens: the lock at start, and the PIN settings of the
 * Control Center (create with confirmation, change, disable, lock now).
 * Storage and checks: pin.c. No default PIN: nothing is locked until the user
 * creates a PIN. */
#include <string.h>
#include "sti_v2_ui.h"

#define ENTRY_OK 1
#define ENTRY_CANCEL 0
#define ENTRY_QUIT -2

typedef struct {
  const char *title, *prompt;
  char text[STI_RECOVERY_LEN + 1];
  int len, max;
  bool recovery_hint; /* "Toolbox: recovery code" (lock screen) */
  char msg[48];
  bool msg_error;
  uint32_t wait_until; /* np_millis() before which input is refused */
} entry_t;

static void entry_scene(void *ctx) {
  entry_t *e = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header(e->title, " ");
  if (gfx_y1 > 40 && gfx_y0 < 100) {
    v2_hex(SCREEN_W / 2, 62, 22, p->acc, 256);
    v2_glyph(V2_GLYPH_LOCK, SCREEN_W / 2, 63, p->dark);
  }
  gfx_text_center(&np_font_body, SCREEN_W / 2, 108, e->prompt, p->text, 256);
  /* one dot per digit: filled when typed */
  int n = e->max, gap = n > 8 ? 16 : 22, x0 = SCREEN_W / 2 - (n - 1) * gap / 2;
  for (int i = 0; i < n; i++) {
    int cx = x0 + i * gap;
    if (e->max == STI_RECOVERY_LEN && i < e->len) {
      char d[2] = {e->text[i], 0};
      gfx_text_center(&np_font_body, cx, 140, d, p->acc, 256);
    } else if (i < e->len) {
      gfx_circle(cx, 135, 5, p->acc, 256);
    } else {
      gfx_circle(cx, 135, 5, p->line, 256);
      gfx_circle(cx, 135, 3, p->bg, 256);
    }
  }
  uint32_t now = np_millis();
  if ((int32_t)(e->wait_until - now) > 0) {
    char w[40], *o = v2_put(w, "Trop d'essais : patiente ");
    o = v2_put_uint(o, (e->wait_until - now + 999) / 1000);
    v2_put(o, " s");
    gfx_text_center(&np_font_small, SCREEN_W / 2, 168, w, RGB(240, 80, 70), 256);
  } else if (e->msg[0]) {
    gfx_text_center(&np_font_small, SCREEN_W / 2, 168, e->msg, e->msg_error ? RGB(240, 80, 70) : p->dim, 256);
  }
  if (e->recovery_hint)
    gfx_text_center(&np_font_small, SCREEN_W / 2, 196, "Code oublie ? Toolbox : code de recuperation", p->dim, 220);
  v2_footer("Chiffres   OK valider   <- effacer   Back annuler");
}

/* Lets the user type up to e->max digits; ENTRY_OK with e->text, ENTRY_CANCEL
 * (Back), ENTRY_QUIT (Home), or 2 for Toolbox when recovery_hint is set. */
static int entry_run(entry_t *e) {
  v2_keys_t k;
  v2_keys_init(&k);
  e->len = 0;
  e->text[0] = 0;
  uint32_t shown_wait = 0;
  bool dirty = true;
  for (;;) {
    uint32_t now = np_millis();
    bool waiting = (int32_t)(e->wait_until - now) > 0;
    uint32_t left = waiting ? (e->wait_until - now + 999) / 1000 : 0;
    if (left != shown_wait) shown_wait = left, dirty = true;
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return ENTRY_QUIT;
    if (v2_is_back(p)) {
      if (e->len) {
        e->len = 0, e->text[0] = 0, dirty = true;
      } else {
        return ENTRY_CANCEL;
      }
    }
    if (e->recovery_hint && (p & V2_KB(eadk_key_toolbox))) return 2;
    if (!waiting) {
      int d = v2_digit(p);
      if (d >= 0 && e->len < e->max) {
        e->text[e->len++] = (char)('0' + d);
        e->text[e->len] = 0;
        e->msg[0] = 0;
        dirty = true;
      }
      if ((p & V2_KB(eadk_key_backspace)) && e->len) {
        e->text[--e->len] = 0;
        dirty = true;
      }
      if (v2_is_ok(p) && e->len) return ENTRY_OK;
    }
    if (dirty) {
      ui_frame(entry_scene, e);
      dirty = false;
    } else {
      np_sleep(15);
    }
  }
}

static void entry_setup(entry_t *e, const char *title, const char *prompt, int max) {
  memset(e, 0, sizeof *e);
  e->title = title;
  e->prompt = prompt;
  e->max = max;
}

static void set_wait(entry_t *e) {
  uint32_t s = sti_v2_pin_wait_seconds(sti_v2_pin_failures());
  e->wait_until = np_millis() + s * 1000;
}

static void fail_msg(entry_t *e, const char *what) {
  int f = sti_v2_pin_failures();
  char *o = v2_put(e->msg, what);
  if (f < STI_PIN_FREE_ATTEMPTS) {
    o = v2_put(o, " - encore ");
    o = v2_put_uint(o, (uint32_t)(STI_PIN_FREE_ATTEMPTS - f));
    v2_put(o, f == STI_PIN_FREE_ATTEMPTS - 1 ? " essai libre" : " essais libres");
  }
  e->msg_error = true;
  set_wait(e);
}

/* shows the recovery code once, in groups of four digits */
static void show_recovery(const char *code) {
  char line[24], *o = line;
  for (int i = 0; i < STI_RECOVERY_LEN; i++) {
    if (i && i % 4 == 0) *o++ = ' ';
    *o++ = code[i];
  }
  *o = 0;
  v2_message("Code de recuperation", line,
             "Note-le maintenant : il ne sera plus jamais affiche. Il permet d'entrer si tu oublies ton PIN.");
}

/* ------------------------------------------------------------------ lock */
int sti_v2_pin_unlock(void) {
  if (!sti_v2_pin_enabled()) return 0;
  entry_t e;
  entry_setup(&e, "STI2D-OS verrouille", "Entre ton code PIN", STI_PIN_MAX_LEN);
  e.recovery_hint = true;
  set_wait(&e);
  for (;;) {
    int r = entry_run(&e);
    if (r == ENTRY_QUIT || r == ENTRY_CANCEL) return STI_V2_QUIT;
    if (r == 2) {
      entry_t rc;
      entry_setup(&rc, "Recuperation", "Code de recuperation (12 chiffres)", STI_RECOVERY_LEN);
      set_wait(&rc);
      for (;;) {
        int rr = entry_run(&rc);
        if (rr == ENTRY_QUIT) return STI_V2_QUIT;
        if (rr == ENTRY_CANCEL) break;
        if (sti_v2_pin_recovery_check(rc.text)) {
          memset(rc.text, 0, sizeof rc.text);
          sti_v2_pin_force_clear();
          v2_message("PIN supprime", "Le code de recuperation est correct : le PIN est retire.",
                     "Cree un nouveau PIN dans Centre de controle > Code PIN.");
          return 0;
        }
        fail_msg(&rc, "Code de recuperation incorrect");
      }
      set_wait(&e);
      continue;
    }
    bool ok = sti_v2_pin_check(e.text);
    memset(e.text, 0, sizeof e.text);
    if (ok) return 0;
    fail_msg(&e, "Code incorrect");
  }
}

/* ------------------------------------------------------------------ settings */
/* asks for a new PIN twice and sets it; true when set */
static int create_pin(void) {
  entry_t a, b;
  for (;;) {
    entry_setup(&a, "Nouveau code PIN", "Choisis exactement 4 chiffres", STI_PIN_MAX_LEN);
    int r = entry_run(&a);
    if (r != ENTRY_OK) return r;
    if (!sti_v2_pin_valid_format(a.text)) {
      v2_message("Code trop court", "Le code PIN doit contenir exactement 4 chiffres.", 0);
      continue;
    }
    entry_setup(&b, "Nouveau code PIN", "Retape le meme code", STI_PIN_MAX_LEN);
    r = entry_run(&b);
    if (r != ENTRY_OK) {
      memset(a.text, 0, sizeof a.text);
      return r;
    }
    bool same = !strcmp(a.text, b.text);
    if (!same) {
      memset(a.text, 0, sizeof a.text);
      memset(b.text, 0, sizeof b.text);
      v2_message("Codes differents", "Les deux codes ne sont pas identiques. Recommence.", 0);
      continue;
    }
    char recovery[STI_RECOVERY_LEN + 1];
    bool ok = sti_v2_pin_set(a.text, recovery);
    memset(a.text, 0, sizeof a.text);
    memset(b.text, 0, sizeof b.text);
    if (!ok) {
      v2_message("Echec", "Le code n'a pas pu etre enregistre (memoire de la calculatrice pleine ?).", 0);
      return ENTRY_CANCEL;
    }
    show_recovery(recovery);
    memset(recovery, 0, sizeof recovery);
    return ENTRY_OK;
  }
}

/* asks for the current PIN: ENTRY_OK when right */
static int ask_current(const char *title) {
  entry_t e;
  entry_setup(&e, title, "Code PIN actuel", STI_PIN_MAX_LEN);
  set_wait(&e);
  for (;;) {
    int r = entry_run(&e);
    if (r != ENTRY_OK) return r;
    bool ok = sti_v2_pin_check(e.text);
    memset(e.text, 0, sizeof e.text);
    if (ok) return ENTRY_OK;
    fail_msg(&e, "Code incorrect");
  }
}

typedef struct {
  int row, rows;
} pins_t;

static void pin_settings_scene(void *ctx) {
  pins_t *s = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header("Code PIN", NULL);
  bool on = sti_v2_pin_enabled();
  int y = V2_HEAD_H + 10;
  v2_panel(10, y, SCREEN_W - 20, 46, 10);
  v2_glyph(V2_GLYPH_LOCK, 34, y + 23, on ? p->acc : p->dim);
  gfx_text(&np_font_body, 58, y + 20, on ? "Code PIN actif" : "Aucun code PIN", p->text, 256);
  gfx_text(&np_font_small, 58, y + 36,
           on ? "Demande a chaque ouverture de STI2D-OS" : "STI2D-OS s'ouvre sans code", p->dim, 256);
  y += 56;
  if (on) {
    v2_row(y, 26, s->row == 0, "Modifier le code", 0, false);
    v2_row(y + 32, 26, s->row == 1, "Desactiver le code", 0, false);
    v2_row(y + 64, 26, s->row == 2, "Verrouiller maintenant", 0, false);
  } else {
    v2_row(y, 26, s->row == 0, "Creer un code PIN", 0, false);
  }
  gfx_paragraph(&np_font_small, SCREEN_W / 2, 196, SCREEN_W - 30, 12,
                "Le PIN protege l'interface, pas les autres apps de la calculatrice.", p->dim, 220);
  v2_footer("Haut/Bas choisir   OK ouvrir   Back retour");
}

int sti_v2_pin_settings(void) {
  pins_t s = {0, 1};
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    bool on = sti_v2_pin_enabled();
    s.rows = on ? 3 : 1;
    if (s.row >= s.rows) s.row = 0;
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (p & V2_KB(eadk_key_up)) s.row = (s.row + s.rows - 1) % s.rows, dirty = true;
    if (p & V2_KB(eadk_key_down)) s.row = (s.row + 1) % s.rows, dirty = true;
    if (v2_is_ok(p)) {
      int r = ENTRY_CANCEL;
      if (!on) {
        r = create_pin();
      } else if (s.row == 0) {
        r = ask_current("Modifier le code");
        if (r == ENTRY_OK) r = create_pin();
      } else if (s.row == 1) {
        r = ask_current("Desactiver le code");
        if (r == ENTRY_OK) {
          sti_v2_pin_force_clear();
          v2_message("Code PIN desactive", "STI2D-OS s'ouvrira sans code.", 0);
        }
      } else {
        r = sti_v2_pin_unlock() == STI_V2_QUIT ? ENTRY_QUIT : ENTRY_OK;
      }
      if (r == ENTRY_QUIT) return STI_V2_QUIT;
      v2_keys_init(&k);
      dirty = true;
    }
    if (dirty) {
      ui_frame(pin_settings_scene, &s);
      dirty = false;
    } else {
      np_sleep(15);
    }
  }
}

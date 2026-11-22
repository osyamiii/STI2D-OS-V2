/* STI2D-OS V2 — Control Center.
 *
 * Every row acts and is saved at once (sti2dos.py for the V2 preferences,
 * sti2dplay.py for the theme shared with STI2D Classic):
 *   Interface, Theme, Mode, Accent, Animations, Fond d'ecran, Style anime,
 *   Code PIN (screen), Informations systeme (screen), Parametres avances
 *   (STI2D-Play's reset / uninstall screen). */
#include <string.h>
#include "sti_v2_ui.h"
#include "sys.h"
#include "live.h"
#if STI_USERLAND
#include "sti_userland.h"
#endif

#if STI_USERLAND
/* userland: the official settings (language, exam mode, brightness...) are one row away */
enum {
  R_ENV, R_THEME, R_MODE, R_ACCENT, R_ANIM, R_WALL, R_LIVE, R_PIN, R_SYSTEM, R_EPSILON, R_ADVANCED, N_ROWS
};
#else
enum {
  R_ENV, R_THEME, R_MODE, R_ACCENT, R_ANIM, R_WALL, R_LIVE, R_PIN, R_SYSTEM, R_ADVANCED, N_ROWS
};
#endif
#define ROW_H 24
#define ROW_STEP 27
#define VISIBLE 7

typedef struct {
  int row, top;
} cc_t;

static const char *row_label(int r) {
  static const char *const l[N_ROWS] = {"Interface", "Theme", "Mode", "Accent", "Animations",
                                        "Fond d'ecran", "Style anime", "Code PIN", "Informations systeme",
#if STI_USERLAND
                                        "Parametres NumWorks",
#endif
                                        "Parametres avances"};
  return l[r];
}

static const char *row_value(int r, bool *arrows) {
  const sti_v2_cfg_t *c = sti_v2_cfg();
  *arrows = true;
  switch (r) {
    case R_ENV: return sti_v2_env_name((sti_env_t)c->env);
    case R_THEME: return sti_theme_name(sti_theme_get());
    case R_MODE: return sti_light_get() ? "Clair" : "Sombre";
    case R_ACCENT: return sti_v2_accent_name(c->accent);
    case R_ANIM: return c->animations ? "Oui" : "Non";
    case R_WALL: return sti_v2_wallpaper_name(c->wallpaper);
    case R_LIVE: return live_ready() ? live_name(live_mode() < LIVE_COUNT ? live_mode() : 0) : "-";
    case R_PIN: *arrows = false; return sti_v2_pin_enabled() ? "Actif  >" : "Inactif  >";
    case R_SYSTEM: *arrows = false; return ">";
#if STI_USERLAND
    case R_EPSILON: *arrows = false; return "Langue, examen...  >";
#endif
    default: *arrows = false; return "Reset, desinstaller  >";
  }
}

static void cc_scene(void *ctx) {
  cc_t *s = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header("Centre de controle", NULL);
  for (int i = 0; i < VISIBLE; i++) {
    int r = s->top + i;
    if (r >= N_ROWS) break;
    int y = V2_HEAD_H + 6 + i * ROW_STEP;
    bool arrows;
    const char *v = row_value(r, &arrows);
    v2_row(y, ROW_H, r == s->row, row_label(r), v, arrows);
    if (r == R_ACCENT && y < gfx_y1 && y + ROW_H > gfx_y0) {
      /* a swatch of the accent in use */
      gfx_rrect(150, y + 6, 12, 12, 4, p->acc, 256);
    }
  }
  /* scroll bar */
  int h = VISIBLE * ROW_STEP - 3, bh = h * VISIBLE / N_ROWS, by = V2_HEAD_H + 6 + (h - bh) * s->top / (N_ROWS - VISIBLE);
  gfx_rrect(SCREEN_W - 6, V2_HEAD_H + 6, 3, h, 1, p->panel, 256);
  gfx_rrect(SCREEN_W - 6, by, 3, bh, 1, p->dim, 256);
  v2_footer("Haut/Bas choisir   < > modifier   OK ouvrir   Back");
}

/* ------------------------------------------------------------------ system info */
/* Lines of system information, shared with the terminal. */
int sti_v2_sysinfo(char lines[][44], int max) {
  int n = 0;
  char *o;
#define LINE() (n < max ? (o = lines[n++]) : (o = lines[max - 1]))
  LINE();
  o = v2_put(o, "STI2D-OS ");
  o = v2_put(o, STI2D_V2_VERSION_STRING);
  o = v2_put(o, " (build ");
  o = v2_put(o, sti_v2_target_name());
  v2_put(o, ")");
  LINE();
  o = v2_put(o, "Interface : ");
  v2_put(o, sti_v2_env_name(sti_v2_env()));
  const np_userland_t *u = np_userland();
  LINE();
  if (u) {
    o = v2_put(o, "Calculatrice : ");
    v2_put(o, np_slow_model() ? "N0110/N0115 (216 MHz)" : "N0120 (550 MHz)");
  } else {
    v2_put(o, "Calculatrice : non identifiee");
  }
  LINE();
  o = v2_put(o, "Logiciel : ");
  if (u) {
    char ver[9];
    memcpy(ver, u->version, 8);
    ver[8] = 0;
    o = v2_put(o, np_upsilon() ? "Upsilon " : "Epsilon ");
    v2_put(o, ver);
  } else {
    v2_put(o, "inconnu (simulateur)");
  }
  LINE();
  int b = np_battery_level();
  o = v2_put(o, "Batterie : ");
  if (b < 0) o = v2_put(o, "inconnue");
  else o = v2_put(v2_put_uint(o, (uint32_t)(b > 3 ? 3 : b)), "/3");
  if (np_battery_charging()) v2_put(o, ", en charge");
  LINE();
  o = v2_put(o, "Memoire de travail : ");
  o = v2_put_uint(o, np_arena_room() / 1024);
  v2_put(o, " Ko");
  uint32_t used, total;
  LINE();
  if (np_storage_stats(&used, &total)) {
    o = v2_put(o, "Fichiers : ");
    o = v2_put_uint(o, used / 1024);
    o = v2_put(o, " Ko / ");
    o = v2_put_uint(o, total / 1024);
    o = v2_put(o, " Ko (libre ");
    o = v2_put_uint(o, (total - used) / 1024);
    v2_put(o, " Ko)");
  } else {
    v2_put(o, "Fichiers : inaccessibles");
  }
#if STI_USERLAND
  LINE();
  o = v2_put(o, "Apps installees : ");
  v2_put_uint(o, (uint32_t)sti_count_apps());
#else
  LINE();
  o = v2_put(o, "Jeux integres : ");
  v2_put_uint(o, (uint32_t)np_game_count);
#endif
  LINE();
  o = v2_put(o, "Code PIN : ");
  v2_put(o, sti_v2_pin_enabled() ? "actif" : "inactif");
#undef LINE
  return n;
}

typedef struct {
  char lines[10][44];
  int n;
} sys_t;

static void sys_scene(void *ctx) {
  sys_t *s = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header("Informations systeme", NULL);
  v2_panel(10, V2_HEAD_H + 8, SCREEN_W - 20, 18 + s->n * 18, 10);
  for (int i = 0; i < s->n; i++) {
    const char *l = s->lines[i];
    const char *colon = strstr(l, " : ");
    int y = V2_HEAD_H + 26 + i * 18;
    if (colon && i > 0) {
      char key[32];
      int k = (int)(colon - l);
      if (k > 31) k = 31;
      memcpy(key, l, (size_t)k);
      key[k] = 0;
      gfx_text(&np_font_small, 22, y, key, p->dim, 256);
      gfx_text_right(&np_font_small, SCREEN_W - 22, y, colon + 3, p->text, 256);
    } else {
      gfx_text(&np_font_body, 22, y, l, p->acc, 256);
    }
  }
  v2_footer("Back retour");
}

static int system_screen(void) {
  static sys_t s;
  s.n = sti_v2_sysinfo(s.lines, 10);
  v2_keys_t k;
  v2_keys_init(&k);
  ui_frame(sys_scene, &s);
  uint32_t last = np_millis();
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p) || v2_is_ok(p)) return STI_V2_BACK;
    if (np_millis() - last > 2000) { /* battery and charging change */
      last = np_millis();
      s.n = sti_v2_sysinfo(s.lines, 10);
      ui_frame(sys_scene, &s);
    }
    np_sleep(20);
  }
}

/* ------------------------------------------------------------------ actions */
/* returns STI_V2_SWITCH_ENV to leave for STI2D Classic, else 0 */
static int change(int r, int d) {
  sti_v2_cfg_t *c = sti_v2_cfg_mut();
  switch (r) {
    case R_ENV:
      sti_v2_set_env(c->env == STI_ENV_OS ? STI_ENV_CLASSIC : STI_ENV_OS);
      return STI_V2_SWITCH_ENV;
    case R_THEME: sti_theme_set(sti_theme_get() + d); break;
    case R_MODE: sti_light_set(!sti_light_get()); break;
    case R_ACCENT:
      c->accent = (uint8_t)((c->accent + d + STI_ACCENT_COUNT) % STI_ACCENT_COUNT);
      sti_v2_save();
      sti_apply_theme();
      break;
    case R_ANIM:
      c->animations = !c->animations;
      sti_v2_save();
      break;
    case R_WALL: {
      int w = c->wallpaper;
      for (int i = 0; i < STI_WALL_COUNT; i++) {
        w = (w + d + STI_WALL_COUNT) % STI_WALL_COUNT;
        if (w != STI_WALL_IMAGE || c->wall_image[0]) break; /* Image: chosen in the Gallery */
      }
      c->wallpaper = (uint8_t)w;
      sti_v2_save();
      break;
    }
    case R_LIVE:
      if (live_ready()) {
        int m = live_mode() < LIVE_COUNT ? live_mode() : 0;
        m = (m + d + LIVE_COUNT) % LIVE_COUNT;
        live_set(m);
        np_bg_save(m);
        if (c->wallpaper != STI_WALL_LIVE) {
          c->wallpaper = STI_WALL_LIVE;
          sti_v2_save();
        }
      }
      break;
    default: break;
  }
  return 0;
}

int sti_v2_control_center(void) {
  cc_t s = {0, 0};
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (p & V2_KB(eadk_key_up)) s.row = (s.row + N_ROWS - 1) % N_ROWS, dirty = true;
    if (p & V2_KB(eadk_key_down)) s.row = (s.row + 1) % N_ROWS, dirty = true;
    int d = (p & V2_KB(eadk_key_left)) ? -1 : (p & V2_KB(eadk_key_right)) ? 1 : 0;
    if (!d && v2_is_ok(p) && s.row <= R_LIVE) d = 1;
    if (d && s.row <= R_LIVE) {
      if (change(s.row, d) == STI_V2_SWITCH_ENV) return STI_V2_SWITCH_ENV;
      dirty = true;
    }
    if (v2_is_ok(p) && s.row > R_LIVE) {
      int r = STI_V2_BACK;
      if (s.row == R_PIN) r = sti_v2_pin_settings();
      else if (s.row == R_SYSTEM) r = system_screen();
#if STI_USERLAND
      else if (s.row == R_EPSILON) {
        int i = sti_host_builtin_settings();
        if (i >= 0) return STI_BUILTIN_BASE - i;
      }
#endif
      else {
        np_settings(); /* STI2D-Play: reset progress, uninstall games */
        np_progress_backup();
        ui_init();
        sti_apply_theme();
      }
      if (r == STI_V2_QUIT) return STI_V2_QUIT;
      v2_keys_init(&k);
      dirty = true;
    }
    if (s.row < s.top) s.top = s.row;
    if (s.row >= s.top + VISIBLE) s.top = s.row - VISIBLE + 1;
    if (dirty) {
      ui_frame(cc_scene, &s);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

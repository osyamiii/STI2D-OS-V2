/* STI2D-OS V2 — the home screen: a honeycomb of the apps.
 *
 * App build (.nwa):
 *   row 0      Jeux    Favoris   Matrices IA
 *   row 1   Mes apps  Horloge  Calendrier  Galerie
 *   row 2      Terminal  Centre   Credits
 *
 * Userland build (STI_USERLAND, the calculator's home screen):
 *   row 0   Calculatrice  Jeux  Favoris  Matrices IA
 *   row 1        Mes apps  Horloge  Calendrier
 *   row 2   Galerie  Terminal  Centre  Credits
 *
 * Arrows move between the cells (up/down go to the nearest cell of the row
 * above/below), OK opens, Back or Home quits to the calculator. Two presses of
 * π in a row switch to STI2D Classic (input_pi.c); one press does nothing. */
#include <string.h>
#include "sti_v2_ui.h"
#include "live.h"

#if STI_USERLAND
enum {
  T_MATHS, T_GAMES, T_FAVS, T_AI, T_APPS, T_CLOCK, T_CAL, T_GALLERY, T_TERM, T_CONTROL, T_FILES, N_TILES
};
#else
enum {
  T_GAMES, T_FAVS, T_AI, T_APPS, T_CLOCK, T_CAL, T_GALLERY, T_TERM, T_CONTROL, T_FILES, N_TILES
};
#endif

static const struct {
  const char *name, *sub;
  uint8_t glyph, row;
  int8_t col2; /* column, in half cells from the centre */
} tiles[N_TILES] = {
#if STI_USERLAND
    {"Calculatrice", "Calculs, Grapheur, Python...", V2_GLYPH_MATHS, 0, -3},
    {"Jeux", "Les jeux installes", V2_GLYPH_GAMES, 0, -1},
    {"Favoris", "Tes jeux preferes", V2_GLYPH_STAR, 0, 1},
    {"Intelligences", "Aelyx IA, Matrices V4, STMG IA", V2_GLYPH_AI, 0, 3},
    {"Mes apps", "Applications ajoutees par toi", V2_GLYPH_APPS, 1, -2},
    {"Horloge", "Heure, chrono, minuteur", V2_GLYPH_CLOCK, 1, 0},
    {"Calendrier", "Mois et evenements", V2_GLYPH_CALENDAR, 1, 2},
    {"Galerie", "Images et fond d'ecran", V2_GLYPH_GALLERY, 2, -3},
    {"Terminal", "Diagnostic du systeme", V2_GLYPH_TERMINAL, 2, -1},
    {"Centre de controle", "Theme, PIN, systeme", V2_GLYPH_CONTROL, 2, 1},
    {"Fichiers", "Lire les documents importes", V2_GLYPH_FILES, 2, 3},
#else
    {"Jeux", "La bibliotheque STI2D-Play", V2_GLYPH_GAMES, 0, -2},
    {"Favoris", "Tes jeux preferes", V2_GLYPH_STAR, 0, 0},
    {"Matrices IA", "L'assistant STI2D", V2_GLYPH_AI, 0, 2},
    {"Mes apps", "Apps installees a cote", V2_GLYPH_APPS, 1, -3},
    {"Horloge", "Heure, chrono, minuteur", V2_GLYPH_CLOCK, 1, -1},
    {"Calendrier", "Mois et evenements", V2_GLYPH_CALENDAR, 1, 1},
    {"Galerie", "Images et fond d'ecran", V2_GLYPH_GALLERY, 1, 3},
    {"Terminal", "Diagnostic du systeme", V2_GLYPH_TERMINAL, 2, -2},
    {"Centre de controle", "Theme, PIN, systeme", V2_GLYPH_CONTROL, 2, 0},
    {"Credits", "L'equipe STI2D", V2_GLYPH_INFO, 2, 2},
#endif
};

#define ICON_R 21
#define CELL_DX 28 /* half the horizontal distance between two cells */
#define ROW_Y0 66
#define ROW_DY 48

static int tile_x(int t) { return SCREEN_W / 2 + tiles[t].col2 * CELL_DX; }
static int tile_y(int t) { return ROW_Y0 + tiles[t].row * ROW_DY; }

typedef struct {
  int sel;
  float s[N_TILES]; /* 0..1: how selected each cell looks (animated) */
  float intro;      /* 0..1 at the opening */
  int minute;       /* last minute drawn in the header */
} home_t;

static home_t h;

static void home_scene(void *ctx) {
  home_t *st = ctx;
  const sti_palette_t *p = sti_palette();
  v2_wallpaper();
  v2_header("STI2D-OS", NULL);
  for (int t = 0; t < N_TILES; t++) {
    int cx = tile_x(t), cy = tile_y(t);
    float s = st->s[t];
    /* V1's staggered 4-3-4 layout is preserved. Selection now magnifies
       the selected app and gently its neighbours, as on a watch home screen. */
    int r = ICON_R + (int)(9 * s + 0.5f);
    if (cy - r - 4 >= gfx_y1 || cy + r + 6 <= gfx_y0) continue;
    int a = (int)(256 * NP_CLAMP(st->intro * 1.6f - (float)tiles[t].row * 0.25f, 0.f, 1.f));
    if (a <= 0) continue;
    if (s > 0.05f) gfx_circle(cx, cy + 3, r + 1, 0, (int)(70 * s) * a / 256);
    color_t fill = gfx_mix(p->panel, p->acc, (int)(32 * s));
    gfx_circle(cx, cy, r, fill, a * (p->light ? 250 : 236) / 256);
    if (s > 0.65f) gfx_circle(cx, cy, r - 2, gfx_mix(fill, p->acc, 18), a * 170 / 256);
    else gfx_circle(cx, cy, r - 2, gfx_mix(fill, p->bg, 6), a * 120 / 256);
    color_t g = s > 0.5f ? p->dark : p->text;
    if (a > 160) v2_glyph(tiles[t].glyph, cx, cy, g);
  }
  if (gfx_y1 > 192 && gfx_y0 < V2_FOOT_Y) {
    int t = st->sel;
    gfx_text_center(&np_font_title, SCREEN_W / 2, 213, tiles[t].name, p->text, 256);
  }
  if (gfx_y1 > V2_FOOT_Y) {
    v2_footer(tiles[st->sel].sub);
  }
}

/* the nearest cell of row `row` to x */
static int nearest(int row, int x) {
  int best = -1, bd = 1 << 30;
  for (int t = 0; t < N_TILES; t++) {
    if (tiles[t].row != row) continue;
    int d = tile_x(t) - x;
    d = d < 0 ? -d * 2 + 1 : d * 2; /* ties go left */
    if (d < bd) bd = d, best = t;
  }
  return best;
}

static int move(int sel, uint64_t p) {
  int row = tiles[sel].row, x = tile_x(sel);
  if (p & V2_KB(eadk_key_left)) {
    for (int t = sel - 1; t >= 0; t--)
      if (tiles[t].row == row) return t;
  }
  if (p & V2_KB(eadk_key_right)) {
    for (int t = sel + 1; t < N_TILES; t++)
      if (tiles[t].row == row) return t;
  }
  if ((p & V2_KB(eadk_key_up)) && row > 0) return nearest(row - 1, x);
  if ((p & V2_KB(eadk_key_down)) && row < 2) return nearest(row + 1, x);
  return sel;
}

/* opens a cell: a game index, STI_QUIT, STI_EXTERNAL, STI_V2_SWITCH_ENV, or -1 to stay */
static int open_tile(int t) {
  int r = -1;
  switch (t) {
#if STI_USERLAND
    case T_MATHS: r = sti_v2_maths(); break;
#endif
    case T_GAMES: r = sti_run_list(false, false); break;
    case T_FAVS: r = sti_run_list(false, true); break;
    case T_APPS: r = sti_run_list(true, false); break;
    case T_AI:
#if STI_USERLAND
      r = sti_ai_open();
      if (r == -2) {
        v2_message("Intelligences", "Aucune IA n'est installee.",
                   "Installe Aelyx IA, Matrices V4 ou STMG IA depuis le portail STI2D-OS.");
        r = -1;
      }
      break;
#endif
      r = sti_matrices_game();
      if (r < 0) {
        v2_message("Intelligences", "Aucune IA compatible n'est installee.",
                   "Installe Aelyx IA, Matrices V4 ou STMG IA depuis le portail STI2D-OS.");
        r = -1;
      }
      break;
    case T_CLOCK: r = sti_v2_clock(); break;
    case T_CAL: r = sti_v2_calendar(); break;
    case T_GALLERY: r = sti_v2_gallery(); break;
    case T_TERM: r = sti_v2_terminal(); break;
    case T_CONTROL: r = sti_v2_control_center(); break;
    case T_FILES: r = sti_v2_files(); break;
    default: break;
  }
  if (r == STI_V2_QUIT) return STI_QUIT;
  return r;
}

int sti_v2_home(bool returning) {
  ui_init();
  sti_apply_theme();
  v2_wallpaper_invalidate();
  bool anim = sti_v2_cfg()->animations;
  if (!returning) {
    memset(&h, 0, sizeof h);
#if STI_USERLAND
    h.sel = T_MATHS;
#else
    h.sel = T_GAMES;
#endif
    h.intro = anim ? 0 : 1;
  } else {
    h.intro = 1;
  }
  for (int t = 0; t < N_TILES; t++) h.s[t] = t == h.sel ? 1.f : 0.f;
  h.minute = -1;
  if (sti_v2_cfg()->wallpaper == STI_WALL_LIVE) v2_live_restore();
  v2_keys_t keys;
  v2_keys_init(&keys);
  uint32_t last = np_millis();
  bool dirty = true;
  for (;;) {
    uint32_t now = np_millis();
    float dt = NP_MIN((now - last) / 1000.f, 0.05f);
    last = now;
    uint64_t p = v2_poll(&keys);
#if STI_USERLAND
    /* this is the calculator's home screen: Home and Back bring the cursor back to the first app */
    if ((v2_is_home(p) || v2_is_back(p)) && h.sel != 0) h.sel = 0, dirty = true;
#else
    if (v2_is_home(p) || v2_is_back(p)) return STI_QUIT;
#endif
    if (sti_v2_pi_feed((p & V2_KB(eadk_key_pi)) != 0, now)) {
      sti_v2_set_env(STI_ENV_CLASSIC);
      return STI_V2_SWITCH_ENV;
    }
    int ns = move(h.sel, p);
    if (ns != h.sel) h.sel = ns, dirty = true;
#if STI_USERLAND
    /* 1-9, as on Epsilon's home screen: the apps of the honeycomb in order */
    int dg = v2_digit(p);
    if (dg >= 1 && dg <= N_TILES && dg - 1 != h.sel) h.sel = dg - 1, dirty = true;
    else if (dg >= 1 && dg <= N_TILES) p |= V2_KB(eadk_key_ok);
#endif
    if (v2_is_ok(p)) {
      int r = open_tile(h.sel);
      if (r != -1) return r;
      /* back on the home screen: the screens may have used the arena and the theme may have changed */
      ui_init();
      sti_apply_theme();
      v2_wallpaper_invalidate();
      anim = sti_v2_cfg()->animations;
      v2_keys_init(&keys);
      sti_v2_pi_reset();
      if (sti_v2_cfg()->wallpaper == STI_WALL_LIVE) v2_live_restore();
      dirty = true;
    }
    /* animation of the selection and of the opening */
    for (int t = 0; t < N_TILES; t++) {
      int dx = tile_x(t) - tile_x(h.sel), dy = tile_y(t) - tile_y(h.sel);
      int d2 = dx * dx + dy * dy;
      float target = t == h.sel ? 1.f : (d2 < 6400 ? (1.f - (float)d2 / 6400.f) * 0.26f : 0.f);
      float v = anim ? ui_approach(h.s[t], target, dt, 0.05f) : target;
      if (v != h.s[t]) h.s[t] = v, dirty = true;
    }
    if (h.intro < 1) {
      h.intro = NP_MIN(h.intro + dt * 2.4f, 1.f);
      dirty = true;
    }
    sti_datetime_t tm = sti_v2_now();
    int minute = tm.valid ? tm.hour * 60 + tm.minute : -1;
    if (minute != h.minute) h.minute = minute, dirty = true;
    bool live = sti_v2_cfg()->wallpaper == STI_WALL_LIVE && live_active() && anim;
    if (live) {
      live_frame(now);
      dirty = true;
    }
    if (dirty) {
      v2_wallpaper_prepare();
      ui_frame(home_scene, &h);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

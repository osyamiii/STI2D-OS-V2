/* STI2D-OS V2 — graphics adaptation layer.
 *
 * The V2 draft had its own gfx_clear/gfx_text/gfx_fill that drew straight to
 * the screen: they clashed with the launcher's gfx_fill/gfx_text (same names,
 * other signatures), so the launcher could not link. Here the V2 screens draw
 * with the launcher's strip renderer instead (launcher/src/gfx.h), and this
 * file adds what they share: theme colours (with the accent of the Control
 * Center), header/footer/rows, hexagons, line glyphs, the wallpaper, modal
 * messages, the keyboard and the alphanumeric line editor. */
#include <string.h>
#include "sti_v2_ui.h"
#include "live.h"
#include "sys.h"

/* ------------------------------------------------------------------ keys */
void v2_keys_init(v2_keys_t *k) {
  k->held = eadk_keyboard_scan();
  k->next = 0;
}

#define V2_REPEAT (V2_ARROWS | V2_KB(eadk_key_backspace))

uint64_t v2_poll(v2_keys_t *k) {
  uint64_t s = eadk_keyboard_scan();
  uint32_t now = np_millis();
  uint64_t pressed = s & ~k->held;
  if (pressed) k->next = now + 380;
  else if ((s & V2_REPEAT) && (int32_t)(now - k->next) >= 0) {
    pressed = s & V2_REPEAT;
    k->next = now + 110;
  }
  k->held = s;
  return pressed;
}

bool v2_is_ok(uint64_t p) { return p & (V2_KB(eadk_key_ok) | V2_KB(eadk_key_exe)); }
bool v2_is_home(uint64_t p) { return p & (V2_KB(eadk_key_home) | V2_KB(eadk_key_on_off)); }
bool v2_is_back(uint64_t p) { return p & V2_KB(eadk_key_back); }

int v2_digit(uint64_t p) {
  static const uint8_t keys[10] = {eadk_key_zero, eadk_key_one, eadk_key_two,   eadk_key_three, eadk_key_four,
                                   eadk_key_five, eadk_key_six, eadk_key_seven, eadk_key_eight, eadk_key_nine};
  for (int i = 0; i < 10; i++)
    if (p & V2_KB(keys[i])) return i;
  return -1;
}

uint64_t v2_wait_press(v2_keys_t *k) {
  for (;;) {
    uint64_t p = v2_poll(k);
    if (p) return p;
    np_sleep(15);
  }
}

/* ------------------------------------------------------------------ accents */
static const struct {
  const char *name;
  uint32_t rgb;
} accents[STI_ACCENT_COUNT] = {
    {"Theme", 0},          {"Orange", 0xFF9800}, {"Rose", 0xFF5C93},  {"Vert", 0x4CC86E}, {"Cyan", 0x22C7E0},
    {"Bleu", 0x4C7DFF},    {"Violet", 0xA66CFF}, {"Rouge", 0xF0503F}, {"Or", 0xF2C14E},
};

uint32_t sti_v2_accent_rgb(int a) { return a > 0 && a < STI_ACCENT_COUNT ? accents[a].rgb : 0; }
const char *sti_v2_accent_name(int a) { return a >= 0 && a < STI_ACCENT_COUNT ? accents[a].name : "?"; }
const char *sti_v2_wallpaper_name(int w) {
  static const char *const n[STI_WALL_COUNT] = {"Uni", "Degrade", "Anime", "Image"};
  return w >= 0 && w < STI_WALL_COUNT ? n[w] : "?";
}

/* ------------------------------------------------------------------ text */
char *v2_put(char *o, const char *s) {
  while (*s) *o++ = *s++;
  *o = 0;
  return o;
}

char *v2_put_uint(char *o, uint32_t v) {
  char d[11];
  int n = 0;
  do d[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n) *o++ = d[--n];
  *o = 0;
  return o;
}

char *v2_put_2(char *o, uint32_t v) {
  *o++ = (char)('0' + v / 10 % 10);
  *o++ = (char)('0' + v % 10);
  *o = 0;
  return o;
}

const char *v2_month_name(int m) {
  static const char *const n[12] = {"Janvier", "Fevrier", "Mars",      "Avril",   "Mai",      "Juin",
                                    "Juillet", "Aout",    "Septembre", "Octobre", "Novembre", "Decembre"};
  return m >= 1 && m <= 12 ? n[m - 1] : "?";
}

const char *v2_day_short(int wd) {
  static const char *const n[7] = {"Lu", "Ma", "Me", "Je", "Ve", "Sa", "Di"};
  return wd >= 0 && wd < 7 ? n[wd] : "?";
}

/* ------------------------------------------------------------------ pieces */
void v2_background(void) { gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, sti_palette()->bg); }

void v2_battery(int x, int y, color_t c) {
  int level = np_battery_level();
  bool charging = np_battery_charging();
  gfx_rrect_outline(x, y, 19, 10, 3, 1, c, 220);
  gfx_fill(x + 19, y + 3, 2, 4, c);
  if (level < 0) {
    gfx_text_center(&np_font_small, x + 10, y + 9, "?", c, 220);
    return;
  }
  int w = level >= 3 ? 15 : level * 5;
  color_t fill = charging ? RGB(76, 200, 110) : level <= 0 ? RGB(240, 80, 70) : c;
  if (w > 0) gfx_rrect(x + 2, y + 2, w, 6, 2, fill, 256);
}

void v2_header(const char *title, const char *right) {
  if (gfx_y0 >= V2_HEAD_H) return;
  const sti_palette_t *p = sti_palette();
  gfx_fill(0, 0, SCREEN_W, V2_HEAD_H - 2, p->panel);
  gfx_fill(0, V2_HEAD_H - 2, SCREEN_W, 2, p->acc);
  gfx_text(&np_font_body, 10, 17, title, p->text, 256);
  if (right) {
    gfx_text_right(&np_font_small, SCREEN_W - 10, 16, right, p->dim, 256);
    return;
  }
  /* status: time (when set) and battery */
  v2_battery(SCREEN_W - 32, 8, p->dim);
  sti_datetime_t t = sti_v2_now();
  if (t.valid) {
    char hm[8], *o = v2_put_2(hm, (uint32_t)t.hour);
    *o++ = ':';
    v2_put_2(o, (uint32_t)t.minute);
    gfx_text_right(&np_font_body, SCREEN_W - 40, 17, hm, p->text, 256);
  }
}

void v2_footer(const char *hint) {
  if (gfx_y1 <= V2_FOOT_Y) return;
  const sti_palette_t *p = sti_palette();
  gfx_fill(0, V2_FOOT_Y, SCREEN_W, SCREEN_H - V2_FOOT_Y, p->panel);
  gfx_fill(0, V2_FOOT_Y, SCREEN_W, 1, p->line);
  gfx_text_center(&np_font_small, SCREEN_W / 2, V2_FOOT_Y + 13, hint, p->dim, 256);
}

void v2_panel(int x, int y, int w, int h, int r) { gfx_rrect(x, y, w, h, r, sti_palette()->panel, 256); }

void v2_row(int y, int h, bool sel, const char *label, const char *value, bool arrows) {
  if (y >= gfx_y1 || y + h <= gfx_y0) return;
  const sti_palette_t *p = sti_palette();
  gfx_rrect(10, y, SCREEN_W - 20, h, 8, sel ? p->acc : p->panel, 256);
  color_t fg = sel ? p->dark : p->text;
  int base = y + h / 2 + 5;
  gfx_text(&np_font_body, 22, base, label, fg, 256);
  if (!value) return;
  if (arrows) {
    int vw = gfx_text_width(&np_font_body, value);
    gfx_icon(&np_icon_left, SCREEN_W - 40 - vw - 12, y + (h - 14) / 2, fg, sel ? 220 : 120);
    gfx_text_right(&np_font_body, SCREEN_W - 40, base, value, fg, 256);
    gfx_icon(&np_icon_right, SCREEN_W - 32, y + (h - 14) / 2, fg, sel ? 220 : 120);
  } else {
    gfx_text_right(&np_font_small, SCREEN_W - 22, base - 1, value, sel ? p->dark : p->dim, 240);
  }
}

/* Pointy-top hexagon of circumradius r, anti-aliased on its slanted edges. */
void v2_hex(int cx, int cy, int r, color_t c, int a) {
  float hw = r * 0.8660254f;
  int y0 = NP_MAX(cy - r, gfx_y0), y1 = NP_MIN(cy + r, gfx_y1);
  for (int y = y0; y < y1; y++) {
    float dy = (float)y + 0.5f - (float)cy;
    if (dy < 0) dy = -dy;
    float half = dy <= r * 0.5f ? hw : hw * (r - dy) / (r * 0.5f);
    if (half <= 0) continue;
    int ih = (int)half;
    float frac = half - (float)ih;
    if (ih > 0) gfx_fill_alpha(cx - ih, y, 2 * ih, 1, c, a);
    int fa = (int)(frac * (float)a);
    if (fa > 8) {
      gfx_fill_alpha(cx - ih - 1, y, 1, 1, c, fa);
      gfx_fill_alpha(cx + ih, y, 1, 1, c, fa);
    }
  }
}

void v2_line(int x0, int y0, int x1, int y1, int t, color_t c) {
  int dx = x1 - x0, dy = y1 - y0;
  int adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
  int steps = NP_MAX(adx, ady) * 2;
  if (steps == 0) steps = 1;
  int lo = t / 2;
  for (int i = 0; i <= steps; i++) {
    int x = x0 + dx * i / steps, y = y0 + dy * i / steps;
    if (y + t - lo < gfx_y0 || y - lo >= gfx_y1) continue;
    gfx_fill(x - lo, y - lo, t, t, c);
  }
}

/* ring of outer radius r and thickness t, anti-aliased */
static void ring(int cx, int cy, int r, int t, color_t c) {
  int y0 = NP_MAX(cy - r - 1, gfx_y0), y1 = NP_MIN(cy + r + 1, gfx_y1);
  float ro = (float)r, ri = (float)(r - t);
  for (int y = y0; y < y1; y++) {
    float fy = (float)y + 0.5f - (float)cy;
    for (int x = cx - r - 1; x <= cx + r; x++) {
      float fx = (float)x + 0.5f - (float)cx;
      float d2 = fx * fx + fy * fy;
      if (d2 > (ro + 1) * (ro + 1) || d2 < (ri - 1) * (ri - 1)) continue;
      /* distance to the ring's band, approximated through d2 */
      float d = d2 > ro * ro ? (d2 - ro * ro) / (2 * ro) : d2 < ri * ri ? (ri * ri - d2) / (2 * (ri > 1 ? ri : 1)) : 0;
      int a = d <= 0 ? 256 : d >= 1 ? 0 : (int)((1 - d) * 256);
      if (a > 0 && x >= 0 && x < SCREEN_W) gfx_fill_alpha(x, y, 1, 1, c, a);
    }
  }
}

void v2_glyph(int id, int cx, int cy, color_t c) {
  switch (id) {
    case V2_GLYPH_GAMES:
      gfx_rrect_outline(cx - 14, cy - 8, 28, 16, 8, 2, c, 256);
      v2_line(cx - 9, cy, cx - 3, cy, 2, c);
      v2_line(cx - 6, cy - 3, cx - 6, cy + 3, 2, c);
      gfx_circle(cx + 5, cy + 2, 2, c, 256);
      gfx_circle(cx + 9, cy - 2, 2, c, 256);
      break;
    case V2_GLYPH_STAR: {
      int px[10], py[10];
      static const int8_t sx[10] = {0, 4, 13, 6, 8, 0, -8, -6, -13, -4};
      static const int8_t sy[10] = {-13, -4, -4, 3, 12, 7, 12, 3, -4, -4};
      for (int i = 0; i < 10; i++) px[i] = cx + sx[i], py[i] = cy + sy[i];
      for (int i = 0; i < 10; i++) v2_line(px[i], py[i], px[(i + 1) % 10], py[(i + 1) % 10], 2, c);
      break;
    }
    case V2_GLYPH_AI:
      gfx_rrect_outline(cx - 9, cy - 9, 18, 18, 4, 2, c, 256);
      for (int i = -4; i <= 4; i += 4) {
        v2_line(cx + i, cy - 13, cx + i, cy - 10, 2, c);
        v2_line(cx + i, cy + 10, cx + i, cy + 13, 2, c);
        v2_line(cx - 13, cy + i, cx - 10, cy + i, 2, c);
        v2_line(cx + 10, cy + i, cx + 13, cy + i, 2, c);
      }
      gfx_rrect(cx - 4, cy - 4, 8, 8, 2, c, 256);
      break;
    case V2_GLYPH_APPS:
      for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) gfx_rrect_outline(cx - 11 + i * 12, cy - 11 + j * 12, 10, 10, 3, 2, c, 256);
      break;
    case V2_GLYPH_CLOCK:
      ring(cx, cy, 13, 2, c);
      v2_line(cx, cy, cx, cy - 8, 2, c);
      v2_line(cx, cy, cx + 6, cy + 3, 2, c);
      break;
    case V2_GLYPH_CALENDAR:
      gfx_rrect_outline(cx - 12, cy - 9, 24, 21, 4, 2, c, 256);
      gfx_fill(cx - 11, cy - 8, 22, 5, c);
      v2_line(cx - 6, cy - 13, cx - 6, cy - 9, 2, c);
      v2_line(cx + 6, cy - 13, cx + 6, cy - 9, 2, c);
      for (int j = 0; j < 2; j++)
        for (int i = 0; i < 3; i++) gfx_fill(cx - 7 + i * 6, cy + 1 + j * 5, 3, 3, c);
      break;
    case V2_GLYPH_GALLERY:
      gfx_rrect_outline(cx - 13, cy - 10, 26, 20, 4, 2, c, 256);
      v2_line(cx - 9, cy + 6, cx - 3, cy - 1, 2, c);
      v2_line(cx - 3, cy - 1, cx + 2, cy + 4, 2, c);
      v2_line(cx + 2, cy + 4, cx + 5, cy + 1, 2, c);
      v2_line(cx + 5, cy + 1, cx + 9, cy + 6, 2, c);
      gfx_circle(cx + 5, cy - 4, 2, c, 256);
      break;
    case V2_GLYPH_TERMINAL:
      gfx_rrect_outline(cx - 13, cy - 10, 26, 20, 4, 2, c, 256);
      v2_line(cx - 8, cy - 4, cx - 4, cy, 2, c);
      v2_line(cx - 4, cy, cx - 8, cy + 4, 2, c);
      v2_line(cx - 1, cy + 5, cx + 7, cy + 5, 2, c);
      break;
    case V2_GLYPH_CONTROL:
      v2_line(cx - 12, cy - 6, cx + 12, cy - 6, 2, c);
      v2_line(cx - 12, cy + 6, cx + 12, cy + 6, 2, c);
      ring(cx - 4, cy - 6, 4, 2, c);
      ring(cx + 5, cy + 6, 4, 2, c);
      break;
    case V2_GLYPH_INFO:
      ring(cx, cy, 13, 2, c);
      gfx_fill(cx - 1, cy - 2, 3, 9, c);
      gfx_fill(cx - 1, cy - 7, 3, 3, c);
      break;
    case V2_GLYPH_FILES:
      gfx_rrect_outline(cx - 10, cy - 13, 20, 26, 3, 2, c, 256);
      v2_line(cx + 3, cy - 13, cx + 10, cy - 6, 2, c);
      v2_line(cx + 3, cy - 13, cx + 3, cy - 6, 2, c);
      v2_line(cx + 3, cy - 6, cx + 10, cy - 6, 2, c);
      v2_line(cx - 6, cy, cx + 6, cy, 2, c);
      v2_line(cx - 6, cy + 5, cx + 6, cy + 5, 2, c);
      break;
    case V2_GLYPH_MATHS:
      gfx_rrect_outline(cx - 11, cy - 13, 22, 26, 4, 2, c, 256);
      gfx_fill(cx - 7, cy - 9, 14, 6, c);
      for (int j = 0; j < 3; j++)
        for (int i = 0; i < 3; i++) gfx_fill(cx - 7 + i * 6, cy + 1 + j * 4, 3, 2, c);
      break;
    case V2_GLYPH_LOCK:
      gfx_rrect(cx - 10, cy - 2, 20, 15, 4, c, 256);
      v2_line(cx - 6, cy - 2, cx - 6, cy - 7, 2, c);
      v2_line(cx + 6, cy - 2, cx + 6, cy - 7, 2, c);
      v2_line(cx - 6, cy - 7, cx - 3, cy - 11, 2, c);
      v2_line(cx + 6, cy - 7, cx + 3, cy - 11, 2, c);
      v2_line(cx - 3, cy - 11, cx + 3, cy - 11, 2, c);
      break;
    default:
      break;
  }
}

/* ------------------------------------------------------------------ wallpaper */
static stimg_t wall_img;
static char wall_name[STI_IMG_NAME_MAX];
static bool wall_ok;

/* Called by the home screen before it renders (decodes the image once). */
void v2_wallpaper_prepare(void) {
  const sti_v2_cfg_t *c = sti_v2_cfg();
  if (c->wallpaper != STI_WALL_IMAGE) return;
  if (wall_ok && !strcmp(wall_name, c->wall_image)) return;
  wall_ok = stimg_load(c->wall_image, &wall_img);
  strncpy(wall_name, c->wall_image, sizeof wall_name - 1);
  wall_name[sizeof wall_name - 1] = 0;
}

void v2_live_restore(void) {
  int m;
  live_set(np_bg_load(&m) && m >= 0 && m < LIVE_COUNT ? m : 0);
}

/* The scratch buffer is shared with the Gallery: it marks the wallpaper stale. */
void v2_wallpaper_invalidate(void) { wall_ok = false; }

void v2_wallpaper(void) {
  const sti_palette_t *p = sti_palette();
  const sti_v2_cfg_t *c = sti_v2_cfg();
  switch (c->wallpaper) {
    case STI_WALL_IMAGE:
      if (wall_ok) {
        stimg_draw(&wall_img, 0, 0, SCREEN_W, SCREEN_H, 120);
        return;
      }
      break;
    case STI_WALL_LIVE:
      if (live_active()) {
        live_strip();
        return;
      }
      break;
    case STI_WALL_PLAIN:
      v2_background();
      return;
    default:
      break;
  }
  /* the whole screen's gradient: every primitive clips itself to the current strip */
  gfx_vgrad(0, 0, SCREEN_W, SCREEN_H, gfx_lerp888(p->top888, p->bg888, p->light ? 120 : 170), p->bg888);
}

/* ------------------------------------------------------------------ modals */
typedef struct {
  const char *title, *l1, *l2;
  bool ask;
} modal_t;

static void modal_scene(void *ctx) {
  modal_t *m = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  int x = 24, y = 56, w = SCREEN_W - 48, h = 128;
  gfx_shadow(x, y + 4, w, h, 14, 10, p->light ? 50 : 140);
  gfx_rrect(x, y, w, h, 14, p->panel, 256);
  gfx_fill(x + 16, y + 34, w - 32, 1, p->line);
  gfx_text_center(&np_font_body, SCREEN_W / 2, y + 24, m->title, p->acc, 256);
  int b = gfx_paragraph(&np_font_small, SCREEN_W / 2, y + 54, w - 32, 13, m->l1, p->text, 256);
  if (m->l2) gfx_paragraph(&np_font_small, SCREEN_W / 2, b + 4, w - 32, 13, m->l2, p->dim, 256);
  if (m->ask) {
    ui_key_hint(x + 30, y + h - 14, "OK", "Confirmer", p->text);
    ui_key_hint(x + w - 110, y + h - 14, "<-", "Annuler", p->dim);
  } else {
    ui_key_hint(SCREEN_W / 2 - 30, y + h - 14, "OK", "Fermer", p->text);
  }
}

static bool modal(const char *title, const char *l1, const char *l2, bool ask) {
  modal_t m = {title, l1, l2, ask};
  v2_keys_t k;
  v2_keys_init(&k);
  ui_frame(modal_scene, &m);
  for (;;) {
    uint64_t p = v2_wait_press(&k);
    if (v2_is_ok(p)) return true;
    if (v2_is_back(p) || v2_is_home(p)) return false;
  }
}

bool v2_confirm(const char *title, const char *l1, const char *l2) { return modal(title, l1, l2, true); }
void v2_message(const char *title, const char *l1, const char *l2) { modal(title, l1, l2, false); }

/* ------------------------------------------------------------------ line editor */
/* NumWorks keyboard. Alpha mode: the letters printed on the keys (a = exp ...
 * z = +), "-" on minus, a space on 0, "." on the dot; Shift held: capitals.
 * Digit mode: digits and the operators. Alpha once: the next key only;
 * Alpha twice: alpha lock; Alpha again: digits. */
char v2_key_char(uint64_t p, bool alpha, bool shift) {
  static const uint8_t letter_keys[26] = {
      eadk_key_exp,    eadk_key_ln,     eadk_key_log,  eadk_key_imaginary,        eadk_key_comma,
      eadk_key_power,  eadk_key_sine,   eadk_key_cosine, eadk_key_tangent,        eadk_key_pi,
      eadk_key_sqrt,   eadk_key_square, eadk_key_seven,  eadk_key_eight,          eadk_key_nine,
      eadk_key_left_parenthesis, eadk_key_right_parenthesis, eadk_key_four, eadk_key_five, eadk_key_six,
      eadk_key_multiplication, eadk_key_division, eadk_key_one, eadk_key_two, eadk_key_three, eadk_key_plus};
  if (alpha) {
    for (int i = 0; i < 26; i++)
      if (p & V2_KB(letter_keys[i])) return (char)((shift ? 'A' : 'a') + i);
    if (p & V2_KB(eadk_key_minus)) return '-';
    if (p & V2_KB(eadk_key_zero)) return ' ';
    if (p & V2_KB(eadk_key_dot)) return '.';
    return 0;
  }
  int d = v2_digit(p);
  if (d >= 0) return (char)('0' + d);
  static const struct {
    uint8_t key;
    char c;
  } ops[] = {{eadk_key_dot, '.'},   {eadk_key_minus, '-'},          {eadk_key_plus, '+'},
             {eadk_key_multiplication, '*'}, {eadk_key_division, '/'}, {eadk_key_left_parenthesis, '('},
             {eadk_key_right_parenthesis, ')'}, {eadk_key_comma, ','}, {eadk_key_power, '^'}};
  for (unsigned i = 0; i < sizeof ops / sizeof ops[0]; i++)
    if (p & V2_KB(ops[i].key)) return ops[i].c;
  return 0;
}

void v2_edit_init(v2_edit_t *e, int cap, bool alpha_lock) {
  memset(e, 0, sizeof *e);
  e->cap = cap < (int)sizeof e->text - 1 ? cap : (int)sizeof e->text - 1;
  e->alpha_lock = alpha_lock;
}

bool v2_edit_feed(v2_edit_t *e, uint64_t p, uint64_t held) {
  if (p & V2_KB(eadk_key_alpha)) {
    if (e->alpha_lock) e->alpha_lock = false, e->alpha_once = false;
    else if (e->alpha_once) e->alpha_lock = true, e->alpha_once = false;
    else e->alpha_once = true;
    return true;
  }
  if (p & V2_KB(eadk_key_backspace)) {
    if (!e->len) return false;
    e->text[--e->len] = 0;
    return true;
  }
  char ch = v2_key_char(p, e->alpha_lock || e->alpha_once, (held & V2_KB(eadk_key_shift)) != 0);
  if (!ch) return false;
  e->alpha_once = false;
  if (e->len >= e->cap) return true;
  e->text[e->len++] = ch;
  e->text[e->len] = 0;
  return true;
}

void v2_edit_draw(const v2_edit_t *e, int x, int y, int w, bool focused) {
  const sti_palette_t *p = sti_palette();
  gfx_rrect(x, y, w, 22, 7, p->panel, 256);
  if (focused) gfx_rrect_outline(x, y, w, 22, 7, 1, p->acc, 256);
  int tx = gfx_text(&np_font_body, x + 8, y + 16, e->text, p->text, 256);
  if (focused) gfx_fill(tx + 1, y + 5, 2, 13, p->acc);
  const char *mode = e->alpha_lock ? "abc" : e->alpha_once ? "a" : "123";
  int mw = gfx_text_width(&np_font_small, mode) + 8;
  gfx_rrect(x + w - mw - 4, y + 4, mw, 14, 5, e->alpha_lock || e->alpha_once ? p->acc : p->line, 256);
  gfx_text_center(&np_font_small, x + w - mw / 2 - 4, y + 15, mode, e->alpha_lock || e->alpha_once ? p->dark : p->text, 256);
}

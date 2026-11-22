/* STI2D-OS V2 — internal UI kit shared by the V2 screens.
 *
 * Built on the launcher's strip renderer (gfx.h / ui.h): a screen is a scene
 * function called once per 16-row strip by ui_frame(). Everything here clips
 * itself to the current strip. Colours come from the launcher's theme
 * (sti_palette()), so STI2D-OS and STI2D Classic always share one theme. */
#ifndef STI_V2_UI_H
#define STI_V2_UI_H
#include <eadk.h>
#include "sti2d_v2.h"
#include "ui.h"
#include "sti2d.h"

#define V2_HEAD_H 28
#define V2_FOOT_Y 222
#define V2_KB(k) ((uint64_t)1 << (k))
#define V2_ARROWS (V2_KB(eadk_key_left) | V2_KB(eadk_key_right) | V2_KB(eadk_key_up) | V2_KB(eadk_key_down))
#define V2_DIGIT_KEYS                                                                                       \
  (V2_KB(eadk_key_zero) | V2_KB(eadk_key_one) | V2_KB(eadk_key_two) | V2_KB(eadk_key_three) |               \
   V2_KB(eadk_key_four) | V2_KB(eadk_key_five) | V2_KB(eadk_key_six) | V2_KB(eadk_key_seven) |               \
   V2_KB(eadk_key_eight) | V2_KB(eadk_key_nine))

/* ---- keyboard: press edges with auto-repeat on arrows and backspace */
typedef struct {
  uint64_t held;
  uint32_t next;
} v2_keys_t;
void v2_keys_init(v2_keys_t *k);
uint64_t v2_poll(v2_keys_t *k);
bool v2_is_ok(uint64_t p);
bool v2_is_home(uint64_t p);
bool v2_is_back(uint64_t p);
int v2_digit(uint64_t p); /* 0..9 or -1 */
/* Waits for the next key press edge (sleeping between scans). */
uint64_t v2_wait_press(v2_keys_t *k);

/* ---- accent colours (Control Center) */
uint32_t sti_v2_accent_rgb(int accent); /* 0 for "theme" */
const char *sti_v2_accent_name(int accent);
const char *sti_v2_wallpaper_name(int w);

/* ---- drawing */
void v2_background(void);                         /* plain fill of the current strip */
void v2_header(const char *title, const char *right);
void v2_footer(const char *hint);
void v2_row(int y, int h, bool selected, const char *label, const char *value, bool arrows);
void v2_panel(int x, int y, int w, int h, int r);
void v2_hex(int cx, int cy, int r, color_t c, int a256);
void v2_line(int x0, int y0, int x1, int y1, int t, color_t c);
/* Homogeneous 2-px line glyphs for the V2 apps, about 26 px wide. */
enum {
  V2_GLYPH_GAMES, V2_GLYPH_STAR, V2_GLYPH_AI, V2_GLYPH_APPS, V2_GLYPH_CLOCK, V2_GLYPH_CALENDAR,
  V2_GLYPH_GALLERY, V2_GLYPH_TERMINAL, V2_GLYPH_CONTROL, V2_GLYPH_INFO, V2_GLYPH_LOCK, V2_GLYPH_MATHS,
  V2_GLYPH_FILES, V2_GLYPH_COUNT
};
void v2_glyph(int id, int cx, int cy, color_t c);
void v2_battery(int x, int y, color_t c); /* 22x10 */
/* The home screen's wallpaper for the current strip (falls back to the gradient). */
void v2_wallpaper(void);
void v2_wallpaper_prepare(void);    /* before rendering: decodes the image wallpaper once */
void v2_wallpaper_invalidate(void); /* the scratch buffer was used by something else */
void v2_live_restore(void);         /* the animated background chosen last (npbg.set) */
/* System information lines (Control Center > Informations systeme, terminal) */
int sti_v2_sysinfo(char lines[][44], int max);
/* A modal message, drawn over a dimmed scene; waits for OK/Back.
 * Returns true for OK. */
bool v2_confirm(const char *title, const char *line1, const char *line2);
void v2_message(const char *title, const char *line1, const char *line2);

/* ---- calendar records (calendar.c), exposed for the host tests */
int sti_v2_cal_parse(const char *s, uint32_t n);
int sti_v2_cal_serialize(char *out, int cap);

/* ---- small text helpers */
char *v2_put(char *o, const char *s);
char *v2_put_uint(char *o, uint32_t v);
char *v2_put_2(char *o, uint32_t v); /* two digits */
const char *v2_month_name(int m);    /* 1..12 */
const char *v2_day_short(int wd);    /* 0 = Monday */

/* ---- alphanumeric line editor, NumWorks keyboard (terminal, calendar) */
typedef struct {
  char text[40];
  int len, cap;
  bool alpha_lock; /* letters on the keys (a = exp ... z = +), else digits */
  bool alpha_once; /* Alpha pressed once: the next key only */
} v2_edit_t;
void v2_edit_init(v2_edit_t *e, int cap, bool alpha_lock);
/* Feeds one press edge; true if the text or the mode changed. */
bool v2_edit_feed(v2_edit_t *e, uint64_t pressed, uint64_t held);
char v2_key_char(uint64_t pressed, bool alpha, bool shift);
void v2_edit_draw(const v2_edit_t *e, int x, int y, int w, bool focused);

/* ---- records (Epsilon file system), through the launcher's storage.c */
typedef bool (*np_record_cb_t)(const char *name, uint32_t size, void *ctx); /* false: stop */
int np_storage_list(const char *prefix, const char *suffix, np_record_cb_t cb, void *ctx);
bool np_storage_stats(uint32_t *used, uint32_t *total);

/* ---- images: stimg_*.py, exported by the web portal (see docs/STIMG.md) */
#define STIMG_MAX_W 160
#define STIMG_MAX_H 120
typedef struct {
  uint16_t w, h;
  uint8_t ncolors;
  color_t palette[16];
  const uint8_t *px; /* w*h palette indices, in ui_scratch */
} stimg_t;
/* Decodes record `name` into the launcher's scratch buffer. */
bool stimg_load(const char *name, stimg_t *img);
/* Pure decoder (host tests): text of the record -> indices in `out`. */
bool stimg_decode(const char *s, uint32_t n, stimg_t *img, uint8_t *out, uint32_t cap);
/* Draws the image scaled into (x, y, w, h) of the current strip, dimmed. */
void stimg_draw(const stimg_t *img, int x, int y, int w, int h, int dim256);
/* ---- userland build only (maths.c, launcher/src/sti2d.c) */
#if STI_USERLAND
int sti_v2_maths(void);   /* STI_BUILTIN_BASE - app, STI_GRID, STI_V2_BACK or STI_V2_QUIT */
int sti_ai_open(void);    /* the AI app (STI_EXTERNAL), the list, or -2 if none is installed */
#endif
#endif

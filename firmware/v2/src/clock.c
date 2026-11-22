/* STI2D-OS V2 — Clock: time of day, stopwatch, timer.
 *
 * Hardware limit, stated on screen: EADK gives apps no battery-backed clock,
 * only a millisecond counter (eadk_timing_millis) that restarts with the
 * calculator. The user sets the date and time once; they advance while the
 * calculator stays on, and must be set again after a restart (the screen
 * says so). The stopwatch and the timer only need the counter.
 * The timer's alert is visual (no speaker on the NumWorks). */
#include <string.h>
#include "sti_v2_ui.h"

enum { TAB_TIME, TAB_STOPWATCH, TAB_TIMER, N_TABS };

/* kept while STI2D-OS runs: the stopwatch and the timer go on in the background */
static struct {
  bool sw_running;
  uint32_t sw_start, sw_acc; /* acc: time before the last start */
  bool tm_running, tm_done;
  uint32_t tm_set, tm_end, tm_left; /* ms */
} st = {false, 0, 0, false, false, 5 * 60000, 0, 5 * 60000};

typedef struct {
  int tab;
  bool alert_on;
} clock_t_;

static uint32_t sw_elapsed(void) { return st.sw_acc + (st.sw_running ? np_millis() - st.sw_start : 0); }

static uint32_t tm_remaining(void) {
  if (!st.tm_running) return st.tm_left;
  int32_t left = (int32_t)(st.tm_end - np_millis());
  return left > 0 ? (uint32_t)left : 0;
}

static char *put_hms(char *o, uint32_t ms, bool cs) {
  uint32_t s = ms / 1000;
  if (s >= 3600) {
    o = v2_put_uint(o, s / 3600);
    *o++ = ':';
  }
  o = v2_put_2(o, s / 60 % 60);
  *o++ = ':';
  o = v2_put_2(o, s % 60);
  if (cs) {
    *o++ = '.';
    o = v2_put_2(o, ms % 1000 / 10);
  }
  *o = 0;
  return o;
}

static const char *weekday_name(int wd) {
  static const char *const n[7] = {"Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi", "Dimanche"};
  return n[wd % 7];
}

static void tabs(int tab) {
  const sti_palette_t *p = sti_palette();
  static const char *const names[N_TABS] = {"Heure", "Chrono", "Minuteur"};
  int w = 92, x0 = (SCREEN_W - N_TABS * w) / 2, y = V2_HEAD_H + 8;
  gfx_rrect(x0 - 3, y - 3, N_TABS * w + 6, 26, 10, p->panel, 256);
  for (int i = 0; i < N_TABS; i++) {
    if (i == tab) gfx_rrect(x0 + i * w, y, w, 20, 8, p->acc, 256);
    gfx_text_center(&np_font_body, x0 + i * w + w / 2, y + 15, names[i], i == tab ? p->dark : p->dim, 256);
  }
}

static void big(const char *s, int y, color_t c) { gfx_text_center(&np_font_title, SCREEN_W / 2, y, s, c, 256); }

static void clock_scene(void *ctx) {
  clock_t_ *c = ctx;
  const sti_palette_t *p = sti_palette();
  if (c->tab == TAB_TIMER && st.tm_done) {
    /* alert: the screen flashes */
    gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, c->alert_on ? p->acc : p->bg);
    color_t fg = c->alert_on ? p->dark : p->text;
    v2_glyph(V2_GLYPH_CLOCK, SCREEN_W / 2, 90, fg);
    gfx_text_center(&np_font_title, SCREEN_W / 2, 140, "Temps ecoule !", fg, 256);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 166, "Appuie sur une touche", fg, 256);
    return;
  }
  v2_background();
  v2_header("Horloge", NULL);
  tabs(c->tab);
  char line[40];
  if (c->tab == TAB_TIME) {
    sti_datetime_t t = sti_v2_now();
    if (t.valid) {
      char *o = v2_put_2(line, (uint32_t)t.hour);
      *o++ = ':';
      o = v2_put_2(o, (uint32_t)t.minute);
      *o++ = ':';
      v2_put_2(o, (uint32_t)t.second);
      big(line, 110, p->text);
      char *d = v2_put(line, weekday_name((uint32_t)sti_v2_weekday(t.year, t.month, t.day)));
      *d++ = ' ';
      d = v2_put_uint(d, (uint32_t)t.day);
      *d++ = ' ';
      d = v2_put(d, v2_month_name(t.month));
      *d++ = ' ';
      v2_put_uint(d, (uint32_t)t.year);
      gfx_text_center(&np_font_body, SCREEN_W / 2, 134, line, p->acc, 256);
    } else {
      big("--:--", 110, p->dim);
      gfx_text_center(&np_font_body, SCREEN_W / 2, 134,
                      sti_v2_cfg()->date ? "La calculatrice a redemarre" : "Heure non reglee", p->acc, 256);
    }
    gfx_paragraph(&np_font_small, SCREEN_W / 2, 164, SCREEN_W - 40, 12,
                  "Pas d'horloge sauvegardee accessible aux apps : l'heure avance tant que la "
                  "calculatrice reste allumee et se regle a nouveau apres un redemarrage.",
                  p->dim, 220);
    v2_footer("OK regler   < > onglet   Back retour");
  } else if (c->tab == TAB_STOPWATCH) {
    put_hms(line, sw_elapsed(), true);
    big(line, 118, st.sw_running ? p->text : p->dim);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 146, st.sw_running ? "En marche" : sw_elapsed() ? "En pause" : "Pret",
                    st.sw_running ? p->acc : p->dim, 256);
    v2_footer("OK marche/pause   <- remise a zero   < > onglet");
  } else {
    put_hms(line, tm_remaining(), false);
    big(line, 118, st.tm_running ? p->text : p->dim);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 146,
                    st.tm_running ? "En marche" : tm_remaining() != st.tm_set ? "En pause" : "Haut/Bas : 1 min   + - : 10 s",
                    st.tm_running ? p->acc : p->dim, 256);
    v2_footer("OK marche/pause   <- remise a zero   < > onglet");
  }
}

/* ------------------------------------------------------------------ set time */
typedef struct {
  int v[5]; /* day, month, year, hour, minute */
  int field;
} set_t;

static void set_scene(void *ctx) {
  set_t *s = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header("Regler la date et l'heure", " ");
  static const char *const labels[5] = {"Jour", "Mois", "Annee", "Heure", "Minute"};
  static const int xs[5] = {30, 90, 150, 222, 274};
  for (int i = 0; i < 5; i++) {
    int w = i == 2 ? 58 : 40, x = xs[i] - w / 2 + (i == 2 ? 6 : 0);
    bool sel = i == s->field;
    gfx_rrect(x, 90, w, 34, 9, sel ? p->acc : p->panel, 256);
    char t[8];
    if (i == 2) v2_put_uint(t, (uint32_t)s->v[i]);
    else v2_put_2(t, (uint32_t)s->v[i]);
    gfx_text_center(&np_font_title, x + w / 2, 115, t, sel ? p->dark : p->text, 256);
    gfx_text_center(&np_font_small, x + w / 2, 140, labels[i], p->dim, 256);
  }
  gfx_text_center(&np_font_body, 120, 112, "/", p->dim, 256);
  gfx_text_center(&np_font_body, 248, 112, ":", p->dim, 256);
  char line[40], *o = v2_put(line, v2_month_name(s->v[1]));
  *o++ = ' ';
  v2_put_uint(o, (uint32_t)s->v[2]);
  gfx_text_center(&np_font_small, SCREEN_W / 2, 74, line, p->acc, 256);
  v2_footer("< > champ   Haut/Bas modifier   OK enregistrer   Back");
}

static int set_time(void) {
  sti_datetime_t t = sti_v2_now();
  const sti_v2_cfg_t *c = sti_v2_cfg();
  set_t s = {{t.day, t.month, t.year, t.valid ? t.hour : 12, t.valid ? t.minute : 0}, 3};
  if (!c->date) s.v[0] = 1, s.v[1] = 9, s.v[2] = 2026;
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  int typed = 0; /* digits typed into the field */
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    static const int lo[5] = {1, 1, 2000, 0, 0}, hi[5] = {31, 12, 2099, 23, 59};
    if (p & V2_KB(eadk_key_left)) s.field = (s.field + 4) % 5, typed = 0, dirty = true;
    if (p & V2_KB(eadk_key_right)) s.field = (s.field + 1) % 5, typed = 0, dirty = true;
    int d = (p & V2_KB(eadk_key_up)) ? 1 : (p & V2_KB(eadk_key_down)) ? -1 : 0;
    if (d) {
      int f = s.field, span = hi[f] - lo[f] + 1;
      s.v[f] = lo[f] + ((s.v[f] - lo[f] + d) % span + span) % span;
      typed = 0;
      dirty = true;
    }
    int dg = v2_digit(p);
    if (dg >= 0) {
      int f = s.field;
      int nv = typed ? s.v[f] * 10 + dg : dg;
      if (nv > hi[f]) nv = dg;
      s.v[f] = nv;
      typed++;
      int maxlen = f == 2 ? 4 : 2;
      if (typed >= maxlen) {
        if (s.v[f] < lo[f]) s.v[f] = lo[f];
        s.field = (s.field + 1) % 5, typed = 0;
      }
      dirty = true;
    }
    int dim = sti_v2_days_in_month(s.v[2] < 2000 ? 2000 : s.v[2], s.v[1]);
    if (s.v[0] > dim) s.v[0] = dim, dirty = true;
    if (v2_is_ok(p)) {
      for (int f = 0; f < 5; f++) s.v[f] = s.v[f] < lo[f] ? lo[f] : s.v[f] > hi[f] ? hi[f] : s.v[f];
      sti_v2_set_datetime(s.v[2], s.v[1], s.v[0], s.v[3], s.v[4]);
      return STI_V2_BACK;
    }
    if (dirty) {
      ui_frame(set_scene, &s);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

/* ------------------------------------------------------------------ main */
int sti_v2_clock(void) {
  clock_t_ c = {TAB_TIME, false};
  if (st.tm_done) c.tab = TAB_TIMER;
  v2_keys_t k;
  v2_keys_init(&k);
  uint32_t last_draw = 0;
  bool dirty = true;
  for (;;) {
    uint32_t now = np_millis();
    uint64_t p = v2_poll(&k);
    if (st.tm_running && tm_remaining() == 0) {
      st.tm_running = false;
      st.tm_left = 0;
      st.tm_done = true;
      c.tab = TAB_TIMER;
      dirty = true;
    }
    if (st.tm_done) {
      if (p) { /* any key stops the alert */
        st.tm_done = false;
        st.tm_left = st.tm_set;
        dirty = true;
        p = 0;
      } else if (now - last_draw > 350) {
        c.alert_on = !c.alert_on;
        dirty = true;
      }
    }
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (p & V2_KB(eadk_key_left)) c.tab = (c.tab + N_TABS - 1) % N_TABS, dirty = true;
    if (p & V2_KB(eadk_key_right)) c.tab = (c.tab + 1) % N_TABS, dirty = true;
    if (c.tab == TAB_TIME && v2_is_ok(p)) {
      if (set_time() == STI_V2_QUIT) return STI_V2_QUIT;
      v2_keys_init(&k);
      dirty = true;
    } else if (c.tab == TAB_STOPWATCH) {
      if (v2_is_ok(p)) {
        if (st.sw_running) st.sw_acc += now - st.sw_start, st.sw_running = false;
        else st.sw_start = now, st.sw_running = true;
        dirty = true;
      }
      if (p & V2_KB(eadk_key_backspace)) {
        st.sw_running = false, st.sw_acc = 0;
        dirty = true;
      }
    } else if (c.tab == TAB_TIMER) {
      if (!st.tm_running) {
        int32_t delta = (p & V2_KB(eadk_key_up)) ? 60000 : (p & V2_KB(eadk_key_down)) ? -60000
                        : (p & V2_KB(eadk_key_plus)) ? 10000 : (p & V2_KB(eadk_key_minus)) ? -10000 : 0;
        if (delta) {
          int32_t v = (int32_t)st.tm_left + delta;
          if (v < 10000) v = 10000;
          if (v > 99 * 60000) v = 99 * 60000;
          st.tm_left = st.tm_set = (uint32_t)v;
          dirty = true;
        }
      }
      if (v2_is_ok(p) && st.tm_left + (st.tm_running ? 1 : 0)) {
        if (st.tm_running) st.tm_left = tm_remaining(), st.tm_running = false;
        else st.tm_end = now + st.tm_left, st.tm_running = true;
        dirty = true;
      }
      if (p & V2_KB(eadk_key_backspace)) {
        st.tm_running = false, st.tm_left = st.tm_set;
        dirty = true;
      }
    }
    /* refresh rate: centiseconds for a running stopwatch, else once a second */
    uint32_t period = (c.tab == TAB_STOPWATCH && st.sw_running) ? 40 : 250;
    if (now - last_draw >= period && !(c.tab == TAB_STOPWATCH && !st.sw_running)) dirty = true;
    if (dirty) {
      ui_frame(clock_scene, &c);
      last_draw = np_millis();
      dirty = false;
    } else {
      np_sleep(10);
    }
  }
}

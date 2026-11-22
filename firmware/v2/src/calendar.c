/* STI2D-OS V2 — Calendar: month view, events (create, edit, delete), the
 * events to come. Events are saved in the Python script stical.py (kept when
 * apps are reinstalled), one line each:  E=yyyymmdd,hhmm,title  ("----" for
 * an event without a time). "Today" is the date set in the Clock. */
#include <string.h>
#include "sti_v2_ui.h"

#define CAL_NAME "stical.py"
#define MAX_EVENTS 40
#define TITLE_MAX 22
#define NO_TIME 0xFFFF

typedef struct {
  uint32_t date;
  uint16_t minute;
  char title[TITLE_MAX + 1];
} event_t;

static event_t ev[MAX_EVENTS];
static int nev;

/* ------------------------------------------------------------------ storage */
static uint32_t parse_u(const char *s, int n) {
  uint32_t v = 0;
  for (int i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return 0xFFFFFFFFu;
    v = v * 10 + (uint32_t)(s[i] - '0');
  }
  return v;
}

/* exposed for the host tests */
int sti_v2_cal_parse(const char *s, uint32_t n) {
  nev = 0;
  for (uint32_t i = 0; i < n && nev < MAX_EVENTS; i++) {
    if ((i && s[i - 1] != '\n' && s[i - 1] != 0) || s[i] != 'E' || i + 16 > n || s[i + 1] != '=') continue;
    const char *p = s + i + 2;
    uint32_t date = parse_u(p, 8);
    if (date == 0xFFFFFFFFu || p[8] != ',' || p[13] != ',') continue;
    uint32_t m = 0;
    if (strncmp(p + 9, "----", 4)) {
      uint32_t hhmm = parse_u(p + 9, 4);
      if (hhmm == 0xFFFFFFFFu || hhmm / 100 > 23 || hhmm % 100 > 59) continue;
      m = hhmm / 100 * 60 + hhmm % 100;
    } else {
      m = NO_TIME;
    }
    event_t *e = &ev[nev];
    e->date = date;
    e->minute = (uint16_t)m;
    const char *t = p + 14;
    int k = 0;
    while (t < s + n && *t != '\n' && *t && k < TITLE_MAX) e->title[k++] = *t++;
    e->title[k] = 0;
    nev++;
  }
  return nev;
}

int sti_v2_cal_serialize(char *out, int cap) {
  char *o = out, *end = out + cap - 48;
  *o++ = 0;
  o = v2_put(o, "# STI2D-OS V2 : calendrier (ne pas modifier a la main)\n");
  for (int i = 0; i < nev && o < end; i++) {
    o = v2_put(o, "E=");
    o = v2_put_uint(o, ev[i].date);
    *o++ = ',';
    if (ev[i].minute == NO_TIME) o = v2_put(o, "----");
    else o = v2_put_2(v2_put_2(o, ev[i].minute / 60u), ev[i].minute % 60u);
    *o++ = ',';
    o = v2_put(o, ev[i].title);
    *o++ = '\n';
  }
  *o++ = 0;
  return (int)(o - out);
}

static void load(void) {
  uint32_t n = 0;
  const char *s = (const char *)np_file_read(CAL_NAME, &n);
  nev = 0;
  if (s && n) sti_v2_cal_parse(s, n);
}

/* the text is made in the launcher's scratch buffer (ui_scratch): static RAM is short on Upsilon */
#define SAVE_CAP (MAX_EVENTS * (TITLE_MAX + 20) + 96)
_Static_assert(SAVE_CAP <= UI_SCRATCH_SIZE, "calendar text must fit ui_scratch");
static bool save(void) {
  if (!ui_scratch) return false;
  char *buf = (char *)ui_scratch;
  v2_wallpaper_invalidate();
  int n = sti_v2_cal_serialize(buf, SAVE_CAP);
  return np_file_write(CAL_NAME, buf, (uint32_t)n);
}

static int cmp(const event_t *a, const event_t *b) {
  if (a->date != b->date) return a->date < b->date ? -1 : 1;
  return (int)a->minute - (int)b->minute;
}

static void sort(void) {
  for (int i = 1; i < nev; i++)
    for (int j = i; j > 0 && cmp(&ev[j], &ev[j - 1]) < 0; j--) {
      event_t t = ev[j];
      ev[j] = ev[j - 1];
      ev[j - 1] = t;
    }
}

static int count_on(uint32_t date) {
  int c = 0;
  for (int i = 0; i < nev; i++) c += ev[i].date == date;
  return c;
}

static uint32_t ymd(int y, int m, int d) { return (uint32_t)(y * 10000 + m * 100 + d); }

/* ------------------------------------------------------------------ month view */
typedef struct {
  int y, m, d;          /* selected day */
  uint32_t today;       /* 0 if the date was never set */
} cal_t;

static char *put_event_time(char *o, const event_t *e) {
  if (e->minute == NO_TIME) return v2_put(o, "--:--");
  o = v2_put_2(o, e->minute / 60u);
  *o++ = ':';
  return v2_put_2(o, e->minute % 60u);
}

static void month_scene(void *ctx) {
  cal_t *c = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header("Calendrier", NULL);
  char title[32], *o = v2_put(title, v2_month_name(c->m));
  *o++ = ' ';
  v2_put_uint(o, (uint32_t)c->y);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 46, title, p->text, 256);
  gfx_icon(&np_icon_left, 60, 36, p->dim, 140);
  gfx_icon(&np_icon_right, SCREEN_W - 68, 36, p->dim, 140);
  const int cw = 42, ch = 21, x0 = (SCREEN_W - 7 * cw) / 2, y0 = 66;
  for (int i = 0; i < 7; i++)
    gfx_text_center(&np_font_small, x0 + i * cw + cw / 2, 61, v2_day_short(i), i >= 5 ? p->acc : p->dim, 256);
  int first = sti_v2_weekday(c->y, c->m, 1), dim = sti_v2_days_in_month(c->y, c->m);
  for (int d = 1; d <= dim; d++) {
    int cell = first + d - 1, col = cell % 7, row = cell / 7;
    int x = x0 + col * cw, y = y0 + row * ch;
    if (y >= gfx_y1 || y + ch <= gfx_y0) continue;
    uint32_t date = ymd(c->y, c->m, d);
    bool sel = d == c->d, today = date == c->today;
    if (sel) gfx_rrect(x + 3, y + 1, cw - 6, ch - 2, 7, p->acc, 256);
    else if (today) gfx_rrect_outline(x + 3, y + 1, cw - 6, ch - 2, 7, 1, p->acc, 256);
    char n[4];
    v2_put_uint(n, (uint32_t)d);
    gfx_text_center(&np_font_body, x + cw / 2, y + 15, n, sel ? p->dark : col >= 5 ? p->dim : p->text, 256);
    if (count_on(date)) gfx_circle(x + cw - 9, y + 5, 2, sel ? p->dark : p->acc, 256);
  }
  /* the next event from the selected day */
  if (gfx_y1 > 196 && gfx_y0 < V2_FOOT_Y) {
    uint32_t from = c->today ? c->today : ymd(c->y, c->m, c->d);
    const event_t *next = 0;
    for (int i = 0; i < nev; i++)
      if (ev[i].date >= from) {
        next = &ev[i];
        break;
      }
    char line[80]; /* (the longest message is 51 characters: 48 overflowed) */
    if (next) {
      o = v2_put(line, "A venir : ");
      o = v2_put_2(o, next->date % 100);
      *o++ = '/';
      o = v2_put_2(o, next->date / 100 % 100);
      *o++ = ' ';
      v2_put(o, next->title);
    } else {
      v2_put(line, c->today ? "Aucun evenement a venir" : "Regle la date dans l'Horloge pour voir aujourd'hui");
    }
    gfx_text_center(&np_font_small, SCREEN_W / 2, 212, line, p->dim, 256);
  }
  v2_footer("Fleches jour  OK ouvrir  Toolbox auj.  Shift a venir");
}

/* ------------------------------------------------------------------ edit form */
typedef struct {
  v2_edit_t title;
  int hour, minute; /* hour -1: no time */
  int field;        /* 0 title, 1 hour, 2 minute */
  uint32_t date;
  bool is_new;
} form_t;

static void form_scene(void *ctx) {
  form_t *f = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header(f->is_new ? "Nouvel evenement" : "Modifier l'evenement", " ");
  char d[32], *o = v2_put_uint(d, f->date % 100);
  *o++ = ' ';
  o = v2_put(o, v2_month_name((int)(f->date / 100 % 100)));
  *o++ = ' ';
  v2_put_uint(o, f->date / 10000);
  gfx_text_center(&np_font_body, SCREEN_W / 2, 52, d, p->acc, 256);
  gfx_text(&np_font_small, 16, 76, "Titre", p->dim, 256);
  v2_edit_draw(&f->title, 12, 82, SCREEN_W - 24, f->field == 0);
  gfx_text(&np_font_small, 16, 124, "Heure (Haut/Bas ; <- : sans heure)", p->dim, 256);
  for (int i = 0; i < 2; i++) {
    int x = 110 + i * 56;
    bool sel = f->field == i + 1;
    gfx_rrect(x, 132, 44, 30, 9, sel ? p->acc : p->panel, 256);
    char t[4];
    if (f->hour < 0) v2_put(t, "--");
    else v2_put_2(t, (uint32_t)(i ? f->minute : f->hour));
    gfx_text_center(&np_font_title, x + 22, 155, t, sel ? p->dark : p->text, 256);
  }
  gfx_text_center(&np_font_title, 160, 153, ":", p->dim, 256);
  v2_footer(f->field ? "< > champ  Haut/Bas valeur  OK enregistrer" : "Bas : l'heure   OK enregistrer   Back annuler");
}

/* edits event `idx` (-1: a new one on `date`); true when saved */
static int edit_event(int idx, uint32_t date) {
  form_t f;
  memset(&f, 0, sizeof f);
  v2_edit_init(&f.title, TITLE_MAX, true);
  f.date = date;
  f.is_new = idx < 0;
  f.hour = 12, f.minute = 0;
  if (idx >= 0) {
    strcpy(f.title.text, ev[idx].title);
    f.title.len = (int)strlen(f.title.text);
    if (ev[idx].minute == NO_TIME) f.hour = -1;
    else f.hour = ev[idx].minute / 60, f.minute = ev[idx].minute % 60;
  }
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (f.field == 0) {
      if (p & V2_KB(eadk_key_down)) f.field = 1, dirty = true;
      else if (!v2_is_ok(p) && p && v2_edit_feed(&f.title, p, k.held)) dirty = true;
    } else {
      /* Left/Right: hour, minute (Left from the hour: back to the title); Up/Down: value */
      if (p & V2_KB(eadk_key_left)) f.field--, dirty = true;
      if ((p & V2_KB(eadk_key_right)) && f.field == 1) f.field = 2, dirty = true;
      int d = (p & V2_KB(eadk_key_up)) ? 1 : (p & V2_KB(eadk_key_down)) ? -1 : 0;
      if (d && f.hour < 0) {
        f.hour = 12, f.minute = 0; /* the first press gives the event a time */
      } else if (d && f.field == 1) {
        f.hour = (f.hour + d + 24) % 24;
      } else if (d && f.field == 2) {
        f.minute = (f.minute + d * 5 + 60) % 60;
      }
      if (d) dirty = true;
      if (p & V2_KB(eadk_key_backspace)) f.hour = -1, dirty = true;
    }
    if (v2_is_ok(p)) {
      if (!f.title.len) {
        v2_message("Titre vide", "Donne un titre a l'evenement.", 0);
        v2_keys_init(&k);
        dirty = true;
        continue;
      }
      if (idx < 0) {
        if (nev >= MAX_EVENTS) {
          v2_message("Calendrier plein", "40 evenements au plus : supprime-en un.", 0);
          return STI_V2_BACK;
        }
        idx = nev++;
      }
      ev[idx].date = f.date;
      ev[idx].minute = f.hour < 0 ? NO_TIME : (uint16_t)(f.hour * 60 + f.minute);
      strncpy(ev[idx].title, f.title.text, TITLE_MAX);
      ev[idx].title[TITLE_MAX] = 0;
      sort();
      if (!save()) v2_message("Echec", "Memoire de la calculatrice pleine : evenement non enregistre.", 0);
      return 1;
    }
    if (dirty) {
      ui_frame(form_scene, &f);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

/* ------------------------------------------------------------------ day / upcoming lists */
typedef struct {
  const char *title;
  int idx[MAX_EVENTS];
  int n, sel, top;
  bool add_row;  /* "+ Nouvel evenement" first */
  bool show_date;
} list_t;

static void list_scene(void *ctx) {
  list_t *l = ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header(l->title, NULL);
  int rows = l->n + (l->add_row ? 1 : 0);
  if (!rows) gfx_text_center(&np_font_body, SCREEN_W / 2, 110, "Aucun evenement", p->dim, 256);
  for (int i = 0; i < 7; i++) {
    int r = l->top + i;
    if (r >= rows) break;
    int y = V2_HEAD_H + 8 + i * 26;
    bool sel = r == l->sel;
    if (l->add_row && r == 0) {
      v2_row(y, 22, sel, "+ Nouvel evenement", 0, false);
      continue;
    }
    const event_t *e = &ev[l->idx[r - (l->add_row ? 1 : 0)]];
    char v[24], *o = v;
    if (l->show_date) {
      o = v2_put_2(o, e->date % 100);
      *o++ = '/';
      o = v2_put_2(o, e->date / 100 % 100);
      *o++ = ' ';
      *o++ = ' ';
    }
    put_event_time(o, e);
    v2_row(y, 22, sel, e->title, v, false);
  }
  v2_footer(l->add_row ? "OK ouvrir   <- supprimer   Back retour" : "OK modifier   <- supprimer   Back retour");
}

static void fill_day(list_t *l, uint32_t date) {
  l->n = 0;
  for (int i = 0; i < nev; i++)
    if (ev[i].date == date) l->idx[l->n++] = i;
}

static void fill_upcoming(list_t *l, uint32_t from) {
  l->n = 0;
  for (int i = 0; i < nev; i++)
    if (ev[i].date >= from) l->idx[l->n++] = i;
}

static int run_list(list_t *l, uint32_t date, uint32_t from) {
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    if (l->add_row) fill_day(l, date);
    else fill_upcoming(l, from);
    int rows = l->n + (l->add_row ? 1 : 0);
    if (l->sel >= rows) l->sel = rows ? rows - 1 : 0;
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (rows) {
      if (p & V2_KB(eadk_key_up)) l->sel = (l->sel + rows - 1) % rows, dirty = true;
      if (p & V2_KB(eadk_key_down)) l->sel = (l->sel + 1) % rows, dirty = true;
    }
    int ev_row = l->sel - (l->add_row ? 1 : 0);
    if (v2_is_ok(p) && rows) {
      int r = ev_row < 0 ? edit_event(-1, date) : edit_event(l->idx[ev_row], ev[l->idx[ev_row]].date);
      if (r == STI_V2_QUIT) return STI_V2_QUIT;
      v2_keys_init(&k);
      dirty = true;
      continue; /* the list is filled again before it is drawn */
    }
    if ((p & V2_KB(eadk_key_backspace)) && ev_row >= 0 && ev_row < l->n) {
      int i = l->idx[ev_row];
      char line[48];
      v2_put(v2_put(v2_put(line, "Supprimer \""), ev[i].title), "\" ?");
      if (v2_confirm("Supprimer l'evenement", line, 0)) {
        memmove(&ev[i], &ev[i + 1], sizeof ev[0] * (size_t)(nev - i - 1));
        nev--;
        save();
      }
      v2_keys_init(&k);
      dirty = true;
      continue;
    }
    if (l->sel < l->top) l->top = l->sel;
    if (l->sel >= l->top + 7) l->top = l->sel - 6;
    if (dirty) {
      ui_frame(list_scene, l);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

/* ------------------------------------------------------------------ main */
int sti_v2_calendar(void) {
  load();
  sort();
  cal_t c;
  sti_datetime_t t = sti_v2_now();
  c.today = sti_v2_cfg()->date ? ymd(t.year, t.month, t.day) : 0;
  c.y = t.year, c.m = t.month, c.d = t.day;
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    int move = (p & V2_KB(eadk_key_left)) ? -1 : (p & V2_KB(eadk_key_right)) ? 1
               : (p & V2_KB(eadk_key_up)) ? -7 : (p & V2_KB(eadk_key_down)) ? 7 : 0;
    if (move) {
      sti_v2_add_days(&c.y, &c.m, &c.d, move);
      if (c.y < 1900) c.y = 1900, c.m = 1, c.d = 1;
      dirty = true;
    }
    if (p & V2_KB(eadk_key_toolbox)) {
      sti_datetime_t n = sti_v2_now();
      c.y = n.year, c.m = n.month, c.d = n.day;
      dirty = true;
    }
    if (v2_is_ok(p) || (p & V2_KB(eadk_key_shift))) {
      static list_t l;
      memset(&l, 0, sizeof l);
      static char title[32];
      int r;
      if (v2_is_ok(p)) {
        char *o = v2_put_uint(title, (uint32_t)c.d);
        *o++ = ' ';
        o = v2_put(o, v2_month_name(c.m));
        *o++ = ' ';
        v2_put_uint(o, (uint32_t)c.y);
        l.title = title;
        l.add_row = true;
        r = run_list(&l, ymd(c.y, c.m, c.d), 0);
      } else {
        l.title = "Evenements a venir";
        l.show_date = true;
        r = run_list(&l, 0, c.today ? c.today : ymd(c.y, c.m, c.d));
      }
      if (r == STI_V2_QUIT) return STI_V2_QUIT;
      v2_keys_init(&k);
      dirty = true;
    }
    if (dirty) {
      ui_frame(month_scene, &c);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

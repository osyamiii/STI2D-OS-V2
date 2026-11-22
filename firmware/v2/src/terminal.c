/* STI2D-OS V2 — Terminal: a diagnostic command interpreter (not a Unix shell).
 *
 * Commands: help, version, env, battery, mem, clear, pin-status, date, uptime,
 *           sysinfo, history, echo <texte>, exit.
 * Typing (v2_edit_*): the keys' letters (alpha lock is on at the start, Alpha
 * switches to digits), Shift held for capitals, <- deletes. Up/Down browse
 * the history, Left/Right scroll the output, OK/EXE runs the command. */
#include <string.h>
#include "sti_v2_ui.h"

#define OUT_LINES 48
#define OUT_COLS 46
#define HIST 8
#define VIEW 13
#define LINE_H 12

enum { TERM_OK = 0, TERM_CLEAR = 1, TERM_EXIT = 2 };

/* the output lines live in the launcher's scratch buffer of the arena (ui_scratch, 19 KB):
 * static RAM is short on Upsilon, and the terminal shows no image meanwhile */
_Static_assert(OUT_LINES * (OUT_COLS + 1) <= UI_SCRATCH_SIZE, "terminal output must fit ui_scratch");
static struct {
  char (*out)[OUT_COLS + 1];
  int count;  /* lines in out, the oldest first */
  int scroll; /* lines scrolled back from the end */
  char hist[HIST][40];
  int nhist, hpos;
  v2_edit_t ed;
} T;

static void out_line(const char *s, void *ctx) {
  (void)ctx;
  /* long lines are cut into screen lines */
  int len = (int)strlen(s);
  int at = 0;
  do {
    if (T.count == OUT_LINES) {
      memmove(T.out[0], T.out[1], sizeof T.out[0] * (OUT_LINES - 1));
      T.count--;
    }
    int n = len - at > OUT_COLS ? OUT_COLS : len - at;
    memcpy(T.out[T.count], s + at, (size_t)n);
    T.out[T.count][n] = 0;
    T.count++;
    at += n;
  } while (at < len);
}

static bool starts(const char *cmd, const char *word) {
  size_t n = strlen(word);
  return !strncmp(cmd, word, n) && (cmd[n] == 0 || cmd[n] == ' ');
}

/* prints the lines of sti_v2_sysinfo whose key starts with one of `keys` ('|' separated) */
static void sysinfo_filtered(const char *keys, sti_term_print_t print, void *ctx) {
  char lines[10][44];
  int n = sti_v2_sysinfo(lines, 10);
  for (int i = 0; i < n; i++) {
    const char *k = keys;
    while (*k) {
      const char *e = strchr(k, '|');
      size_t kl = e ? (size_t)(e - k) : strlen(k);
      if (!strncmp(lines[i], k, kl)) {
        print(lines[i], ctx);
        break;
      }
      k += kl + (e ? 1 : 0);
    }
  }
}

static int exec(const char *cmd, sti_term_print_t print, void *ctx) {
  while (*cmd == ' ') cmd++;
  char line[64];
  if (!cmd[0]) return TERM_OK;
  if (starts(cmd, "help")) {
    print("Commandes :", ctx);
    print("  help      cette aide", ctx);
    print("  version   version de STI2D-OS", ctx);
    print("  env       interface active", ctx);
    print("  battery   niveau de batterie", ctx);
    print("  mem       memoire et fichiers", ctx);
    print("  pin-status  etat du code PIN", ctx);
    print("  date      date et heure reglees", ctx);
    print("  uptime    temps depuis l'allumage", ctx);
    print("  sysinfo   tout le systeme", ctx);
    print("  history   commandes precedentes", ctx);
    print("  echo, clear, exit", ctx);
    print("Alpha : lettres/chiffres. 0 = espace.", ctx);
  } else if (starts(cmd, "version")) {
    char *o = v2_put(line, "STI2D-OS ");
    o = v2_put(o, STI2D_V2_VERSION_STRING);
    o = v2_put(o, " - build ");
    v2_put(o, sti_v2_target_name());
    print(line, ctx);
  } else if (starts(cmd, "env")) {
    char *o = v2_put(line, "interface : ");
    v2_put(o, sti_v2_env_name(sti_v2_env()));
    print(line, ctx);
    print("(double pi sur l'accueil pour changer)", ctx);
  } else if (starts(cmd, "battery")) {
    sysinfo_filtered("Batterie", print, ctx);
  } else if (starts(cmd, "mem")) {
    sysinfo_filtered("Memoire|Fichiers", print, ctx);
  } else if (starts(cmd, "sysinfo")) {
    sysinfo_filtered("STI2D|Interface|Calculatrice|Logiciel|Batterie|Memoire|Fichiers|Jeux|Code", print, ctx);
  } else if (starts(cmd, "pin-status") || starts(cmd, "pin")) {
    if (!sti_v2_pin_enabled()) {
      print("PIN : desactive", ctx);
    } else {
      int f = sti_v2_pin_failures();
      char *o = v2_put(line, "PIN : actif, echecs recents : ");
      v2_put_uint(o, (uint32_t)f);
      print(line, ctx);
      uint32_t w = sti_v2_pin_wait_seconds(f);
      if (w) {
        o = v2_put(line, "attente avant essai : ");
        o = v2_put_uint(o, w);
        v2_put(o, " s");
        print(line, ctx);
      }
    }
  } else if (starts(cmd, "date")) {
    sti_datetime_t t = sti_v2_now();
    if (!sti_v2_cfg()->date) {
      print("date : non reglee (Horloge > OK)", ctx);
    } else {
      char *o = v2_put_2(line, (uint32_t)t.day);
      *o++ = '/';
      o = v2_put_2(o, (uint32_t)t.month);
      *o++ = '/';
      o = v2_put_uint(o, (uint32_t)t.year);
      if (t.valid) {
        *o++ = ' ';
        o = v2_put_2(o, (uint32_t)t.hour);
        *o++ = ':';
        o = v2_put_2(o, (uint32_t)t.minute);
      } else {
        o = v2_put(o, " (heure perdue : redemarrage)");
      }
      print(line, ctx);
    }
  } else if (starts(cmd, "uptime")) {
    uint32_t s = np_millis() / 1000;
    char *o = v2_put(line, "allumee depuis ");
    o = v2_put_uint(o, s / 3600);
    o = v2_put(o, " h ");
    o = v2_put_2(o, s / 60 % 60);
    o = v2_put(o, " min ");
    o = v2_put_2(o, s % 60);
    v2_put(o, " s");
    print(line, ctx);
  } else if (starts(cmd, "history")) {
    if (!T.nhist) print("(vide)", ctx);
    for (int i = 0; i < T.nhist; i++) {
      char *o = v2_put_uint(line, (uint32_t)(i + 1));
      o = v2_put(o, "  ");
      v2_put(o, T.hist[i]);
      print(line, ctx);
    }
  } else if (starts(cmd, "echo")) {
    print(cmd[4] ? cmd + 5 : "", ctx);
  } else if (starts(cmd, "clear")) {
    return TERM_CLEAR;
  } else if (starts(cmd, "exit")) {
    return TERM_EXIT;
  } else {
    char *o = v2_put(line, "commande inconnue : ");
    strncpy(o, cmd, 24);
    o[24] = 0;
    print(line, ctx);
    print("tape help", ctx);
  }
  return TERM_OK;
}

void sti_v2_term_exec(const char *cmd, sti_term_print_t print, void *ctx) { exec(cmd, print, ctx); }

static void term_scene(void *ctx) {
  (void)ctx;
  const sti_palette_t *p = sti_palette();
  color_t bg = p->light ? RGB(250, 250, 252) : RGB(10, 12, 16);
  gfx_fill(0, gfx_y0, SCREEN_W, gfx_y1 - gfx_y0, bg);
  v2_header("Terminal", NULL);
  int first = T.count - VIEW - T.scroll;
  if (first < 0) first = 0;
  for (int i = 0; i < VIEW; i++) {
    int k = first + i;
    if (k >= T.count) break;
    int y = V2_HEAD_H + 14 + i * LINE_H;
    if (y - 10 >= gfx_y1 || y + 3 <= gfx_y0) continue;
    const char *s = T.out[k];
    color_t c = s[0] == '>' ? p->acc : p->text;
    gfx_text(&np_font_small, 8, y, s, c, 256);
  }
  if (T.scroll) {
    char m[24], *o = v2_put(m, "+");
    o = v2_put_uint(o, (uint32_t)T.scroll);
    v2_put(o, " lignes");
    gfx_text_right(&np_font_small, SCREEN_W - 8, V2_HEAD_H + 12, m, p->dim, 256);
  }
  v2_edit_draw(&T.ed, 6, 194, SCREEN_W - 12, true);
  v2_footer("OK executer  Haut/Bas historique  < > defiler");
}

int sti_v2_terminal(void) {
  if (!ui_scratch) return STI_V2_BACK;
  T.out = (char (*)[OUT_COLS + 1])(void *)ui_scratch;
  v2_wallpaper_invalidate();
  T.count = 0;
  T.scroll = 0;
  T.hpos = T.nhist;
  v2_edit_init(&T.ed, 36, true);
  out_line("STI2D-OS " STI2D_V2_VERSION_STRING " - terminal de diagnostic", 0);
  out_line("Tape help puis OK.", 0);
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) return STI_V2_BACK;
    if (v2_is_ok(p)) {
      char line[48] = "> ";
      strncat(line, T.ed.text, sizeof line - 3);
      out_line(line, 0);
      if (T.ed.len) {
        if (T.nhist == HIST) memmove(T.hist[0], T.hist[1], sizeof T.hist[0] * (HIST - 1)), T.nhist--;
        strncpy(T.hist[T.nhist], T.ed.text, sizeof T.hist[0] - 1);
        T.hist[T.nhist][sizeof T.hist[0] - 1] = 0;
        T.nhist++;
      }
      int r = exec(T.ed.text, out_line, 0);
      if (r == TERM_CLEAR) T.count = 0;
      if (r == TERM_EXIT) return STI_V2_BACK;
      bool lock = T.ed.alpha_lock;
      v2_edit_init(&T.ed, 36, lock);
      T.hpos = T.nhist;
      T.scroll = 0;
      dirty = true;
    } else if (p & (V2_KB(eadk_key_up) | V2_KB(eadk_key_down))) {
      if (T.nhist) {
        T.hpos += (p & V2_KB(eadk_key_up)) ? -1 : 1;
        if (T.hpos < 0) T.hpos = 0;
        bool lock = T.ed.alpha_lock;
        v2_edit_init(&T.ed, 36, lock);
        if (T.hpos >= T.nhist) {
          T.hpos = T.nhist;
        } else {
          strncpy(T.ed.text, T.hist[T.hpos], sizeof T.ed.text - 1);
          T.ed.len = (int)strlen(T.ed.text);
        }
        dirty = true;
      }
    } else if (p & (V2_KB(eadk_key_left) | V2_KB(eadk_key_right))) {
      int max = T.count > VIEW ? T.count - VIEW : 0;
      T.scroll += (p & V2_KB(eadk_key_left)) ? 4 : -4;
      T.scroll = T.scroll < 0 ? 0 : T.scroll > max ? max : T.scroll;
      dirty = true;
    } else if (p && v2_edit_feed(&T.ed, p, k.held)) {
      dirty = true;
    }
    if (dirty) {
      ui_frame(term_scene, 0);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

/* STI2D-OS V2 — preferences and date/time.
 *
 * Saved as text in the Python script "sti2dos.py" (like STI2D-Play's
 * sti2dplay.py): the NumWorks installer keeps Python scripts when apps are
 * installed, so the preferences survive updates. The PIN is NOT here
 * (see pin.c). */
#include <string.h>
#include <eadk.h>
#include "sti2d_v2.h"

const uint8_t *np_file_read(const char *name, uint32_t *len);
bool np_file_write(const char *name, const void *data, uint32_t len);

#define CFG_NAME "sti2dos.py"

static sti_v2_cfg_t g_cfg;
static bool g_loaded;

const char *sti_v2_target_name(void) {
#if STI_USERLAND
  switch (STI_TARGET) {
    case STI_TARGET_N0110: return "userland N0110";
    case STI_TARGET_N0115: return "userland N0115";
    case STI_TARGET_N0120: return "userland N0120";
    default: return "userland";
  }
#else
  switch (STI_TARGET) {
    case STI_TARGET_N0110: return "N0110";
    case STI_TARGET_N0115: return "N0115";
    case STI_TARGET_N0120: return "N0120";
    default: return "host";
  }
#endif
}

void sti_v2_cfg_defaults(sti_v2_cfg_t *c) {
  memset(c, 0, sizeof *c);
  c->env = STI_ENV_OS;
  c->accent = 0;
  /* the N0110 and N0115 run at 216 MHz: lighter by default (still a setting) */
  c->animations = (STI_TARGET == STI_TARGET_N0110 || STI_TARGET == STI_TARGET_N0115) ? 0 : 1;
  c->wallpaper = STI_WALL_GRADIENT;
}

/* ------------------------------------------------------------ text format */
static char *put(char *o, char *end, const char *s) {
  while (*s && o < end) *o++ = *s++;
  return o;
}
static char *put_u(char *o, char *end, uint32_t v) {
  char d[11];
  int n = 0;
  do d[n++] = (char)('0' + v % 10), v /= 10;
  while (v);
  while (n && o < end) *o++ = d[--n];
  return o;
}
static char *line_u(char *o, char *end, const char *key, uint32_t v) {
  o = put(o, end, key);
  o = put_u(o, end, v);
  return put(o, end, "\n");
}

int sti_v2_cfg_serialize(const sti_v2_cfg_t *c, char *out, int cap) {
  char *o = out, *end = out + cap - 1;
  *o++ = 0; /* Python's auto-import flag of the script: off */
  o = put(o, end, "# STI2D-OS V2 : reglages (ne pas modifier a la main)\n");
  o = line_u(o, end, "E=", c->env);
  o = line_u(o, end, "A=", c->accent);
  o = line_u(o, end, "N=", c->animations);
  o = line_u(o, end, "W=", c->wallpaper);
  o = put(o, end, "I=");
  o = put(o, end, c->wall_image);
  o = put(o, end, "\n");
  o = line_u(o, end, "D=", c->date);
  o = line_u(o, end, "M=", c->minute_of_day);
  o = line_u(o, end, "S=", c->set_millis);
  *o++ = 0;
  return (int)(o - out);
}

/* the value after "\nKEY=" (or at the start), as text up to the end of the line */
static const char *field(const char *s, uint32_t n, const char *key, uint32_t *vlen) {
  uint32_t kl = (uint32_t)strlen(key);
  for (uint32_t i = 0; i + kl <= n; i++) {
    if (i && s[i - 1] != '\n' && s[i - 1] != 0) continue;
    if (strncmp(s + i, key, kl)) continue;
    uint32_t j = i + kl;
    while (j < n && s[j] != '\n' && s[j] != 0) j++;
    *vlen = j - (i + kl);
    return s + i + kl;
  }
  return 0;
}

static bool field_u(const char *s, uint32_t n, const char *key, uint32_t max, uint32_t *out) {
  uint32_t len;
  const char *v = field(s, n, key, &len);
  if (!v || !len || len > 10) return false;
  uint64_t x = 0;
  for (uint32_t i = 0; i < len; i++) {
    if (v[i] < '0' || v[i] > '9') return false;
    x = x * 10 + (uint64_t)(v[i] - '0');
  }
  if (x > max) return false;
  *out = (uint32_t)x;
  return true;
}

bool sti_v2_cfg_parse(sti_v2_cfg_t *c, const char *s, uint32_t n) {
  if (!s || n < 4) return false;
  uint32_t v, len;
  bool any = false;
  if (field_u(s, n, "E=", STI_ENV_COUNT - 1, &v)) c->env = (uint8_t)v, any = true;
  if (field_u(s, n, "A=", STI_ACCENT_COUNT - 1, &v)) c->accent = (uint8_t)v, any = true;
  if (field_u(s, n, "N=", 1, &v)) c->animations = (uint8_t)v, any = true;
  if (field_u(s, n, "W=", STI_WALL_COUNT - 1, &v)) c->wallpaper = (uint8_t)v, any = true;
  const char *img = field(s, n, "I=", &len);
  if (img && len < STI_IMG_NAME_MAX) {
    memcpy(c->wall_image, img, len);
    c->wall_image[len] = 0;
  }
  if (field_u(s, n, "D=", 99991231u, &v) && (v == 0 || v >= 19000101u)) c->date = v;
  if (field_u(s, n, "M=", 24 * 60 - 1, &v)) c->minute_of_day = v;
  if (field_u(s, n, "S=", 0xFFFFFFFFu, &v)) c->set_millis = v;
  if (c->wallpaper == STI_WALL_IMAGE && !c->wall_image[0]) c->wallpaper = STI_WALL_GRADIENT;
  return any;
}

void sti_v2_load(void) {
  sti_v2_cfg_defaults(&g_cfg);
  uint32_t n = 0;
  const char *d = (const char *)np_file_read(CFG_NAME, &n);
  if (d && n) {
    sti_v2_cfg_t tmp = g_cfg;
    if (sti_v2_cfg_parse(&tmp, d, n)) g_cfg = tmp;
  }
  g_loaded = true;
}

bool sti_v2_save(void) {
  if (!g_loaded) sti_v2_load();
  char buf[256];
  int n = sti_v2_cfg_serialize(&g_cfg, buf, sizeof buf);
  return np_file_write(CFG_NAME, buf, (uint32_t)n);
}

const sti_v2_cfg_t *sti_v2_cfg(void) {
  if (!g_loaded) sti_v2_load();
  return &g_cfg;
}

sti_v2_cfg_t *sti_v2_cfg_mut(void) {
  if (!g_loaded) sti_v2_load();
  return &g_cfg;
}

sti_env_t sti_v2_env(void) { return (sti_env_t)sti_v2_cfg()->env; }

void sti_v2_set_env(sti_env_t env) {
  if (env >= STI_ENV_COUNT) return;
  sti_v2_cfg_mut()->env = (uint8_t)env;
  sti_v2_save();
}

const char *sti_v2_env_name(sti_env_t env) {
  return env == STI_ENV_OS ? "STI2D-OS" : env == STI_ENV_CLASSIC ? "STI2D Classic" : "?";
}

uint32_t sti_random32(void) { return eadk_random(); }

/* ------------------------------------------------------------ date / time */
static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int sti_v2_days_in_month(int y, int m) {
  static const uint8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 30;
  return m == 2 && leap(y) ? 29 : d[m - 1];
}

/* Sakamoto's method, shifted so that Monday is 0 */
int sti_v2_weekday(int y, int m, int d) {
  static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7; /* 0 = Sunday */
  return (w + 6) % 7;
}

void sti_v2_add_days(int *y, int *m, int *d, int days) {
  while (days > 0) {
    int left = sti_v2_days_in_month(*y, *m) - *d;
    if (days <= left) {
      *d += days;
      return;
    }
    days -= left + 1;
    *d = 1;
    if (++*m > 12) *m = 1, ++*y;
  }
  while (days < 0) {
    if (-days < *d) {
      *d += days;
      return;
    }
    days += *d;
    if (--*m < 1) *m = 12, --*y;
    *d = sti_v2_days_in_month(*y, *m);
  }
}

void sti_v2_set_datetime(int year, int month, int day, int hour, int minute) {
  sti_v2_cfg_t *c = sti_v2_cfg_mut();
  if (year < 1900) year = 1900;
  if (year > 9999) year = 9999;
  month = month < 1 ? 1 : month > 12 ? 12 : month;
  int dim = sti_v2_days_in_month(year, month);
  day = day < 1 ? 1 : day > dim ? dim : day;
  c->date = (uint32_t)(year * 10000 + month * 100 + day);
  c->minute_of_day = (uint32_t)((hour % 24) * 60 + minute % 60);
  c->set_millis = (uint32_t)eadk_timing_millis();
  sti_v2_save();
}

sti_datetime_t sti_v2_now(void) {
  const sti_v2_cfg_t *c = sti_v2_cfg();
  sti_datetime_t t = {2026, 1, 1, 0, 0, 0, false};
  if (!c->date) return t;
  t.year = (int)(c->date / 10000), t.month = (int)(c->date / 100 % 100), t.day = (int)(c->date % 100);
  uint32_t now = (uint32_t)eadk_timing_millis();
  /* the counter restarts with the calculator: the base is lost then */
  t.valid = now >= c->set_millis;
  uint32_t elapsed = t.valid ? now - c->set_millis : 0;
  uint32_t total_s = c->minute_of_day * 60 + elapsed / 1000;
  sti_v2_add_days(&t.year, &t.month, &t.day, (int)(total_s / 86400));
  total_s %= 86400;
  t.hour = (int)(total_s / 3600);
  t.minute = (int)(total_s / 60 % 60);
  t.second = (int)(total_s % 60);
  return t;
}

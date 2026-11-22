/* Host harness for STI2D-OS V2: a stand-in for EADK and for the parts of the
 * launcher that need the calculator (file system, game table, arena).
 *
 * - The keyboard follows a script of actions on a virtual clock (key presses
 *   held for 60 ms, waits, screenshots). When the script is over, the next
 *   keyboard scan jumps back to the test (longjmp): the launcher's loops never
 *   end on their own.
 * - The screen is a 320x240 RGB565 frame buffer; snapshots are written as PPM.
 * - The file system is in memory, with the calculator's ~42 KB limit. */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eadk.h>
#include "np.h"
#include "mock.h"

/* ------------------------------------------------------------------ clock & keys */
static uint32_t vt = 1000; /* virtual milliseconds */
static act_t acts[MOCK_MAX_ACTS];
static int nacts, cur;
static uint32_t act_start;
static uint64_t state;
static jmp_buf *escape;
static const char *snap_dir = "build/host-shots";
uint32_t mock_scans;

void mock_set_snap_dir(const char *d) { snap_dir = d; }
uint32_t mock_now(void) { return vt; }
void mock_advance(uint32_t ms) { vt += ms; }

void script_reset(void) {
  nacts = 0;
  cur = 0;
  act_start = vt;
  state = 0;
}
static void push(act_t a) {
  if (nacts < MOCK_MAX_ACTS) acts[nacts++] = a;
}
void key(int k) { push((act_t){ACT_KEY, (uint64_t)1 << k, 60, 90, 0}); }
void key_with(int mod, int k) { push((act_t){ACT_KEY, ((uint64_t)1 << mod) | ((uint64_t)1 << k), 60, 90, 0}); }
void key_n(int k, int n) {
  for (int i = 0; i < n; i++) key(k);
}
void key_hold(int k, int ms) { push((act_t){ACT_KEY, (uint64_t)1 << k, ms, 90, 0}); }
void wait_ms(int ms) { push((act_t){ACT_WAIT, 0, ms, 0, 0}); }
void snap(const char *name) { push((act_t){ACT_SNAP, 0, 0, 0, name}); }

static void dump(const char *name);

static void step_ex(bool in_vblank) {
  for (;;) {
    if (cur >= nacts) {
      state = 0;
      if (vt - act_start > 250 && escape) longjmp(*escape, 1);
      return;
    }
    act_t *a = &acts[cur];
    uint32_t t = vt - act_start;
    if (a->kind == ACT_SNAP) {
      if (in_vblank) return; /* the next frame is not drawn yet: snap at the next key scan */
      dump(a->name);
      cur++, act_start = vt;
      continue;
    }
    uint32_t total = (uint32_t)(a->hold + a->gap);
    if (t >= total) {
      cur++, act_start = vt;
      state = 0;
      continue;
    }
    state = a->kind == ACT_KEY && t < (uint32_t)a->hold ? a->mask : 0;
    return;
  }
}

static void step(void) { step_ex(false); }

int mock_run(int (*fn)(void), int *ret) {
  jmp_buf jb;
  escape = &jb;
  if (setjmp(jb)) {
    escape = 0;
    return 1; /* script over, the screen was still running */
  }
  int r = fn();
  if (ret) *ret = r;
  escape = 0;
  return 0;
}

void _eadk_keyboard_scan_do_scan(void) {
  vt += 1;
  mock_scans++;
  step();
}
uint32_t _eadk_keyboard_scan_low(void) { return (uint32_t)state; }
uint32_t _eadk_keyboard_scan_high(void) { return (uint32_t)(state >> 32); }
uint32_t _eadk_timing_millis_low(void) { return vt; }
uint32_t _eadk_timing_millis_high(void) { return 0; }
void eadk_timing_msleep(uint32_t ms) {
  vt += ms;
  step();
}
void eadk_timing_usleep(uint32_t us) { vt += us / 1000; }
eadk_event_t eadk_event_get(int32_t *timeout) {
  (void)timeout;
  return 0;
}
uint32_t eadk_random(void) {
  static uint32_t x = 0x12345678;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return x;
}
bool eadk_usb_is_plugged(void) { return true; }

/* ------------------------------------------------------------------ screen */
uint16_t mock_fb[240][320];
uint32_t mock_frames;

void eadk_display_push_rect(eadk_rect_t r, const eadk_color_t *px) {
  for (int y = 0; y < r.height; y++)
    for (int x = 0; x < r.width; x++)
      if (r.y + y < 240 && r.x + x < 320) mock_fb[r.y + y][r.x + x] = px[y * r.width + x];
}
void eadk_display_push_rect_uniform(eadk_rect_t r, eadk_color_t c) {
  for (int y = 0; y < r.height; y++)
    for (int x = 0; x < r.width; x++)
      if (r.y + y < 240 && r.x + x < 320) mock_fb[r.y + y][r.x + x] = c;
}
void eadk_display_pull_rect(eadk_rect_t r, eadk_color_t *px) {
  for (int y = 0; y < r.height; y++)
    for (int x = 0; x < r.width; x++) px[y * r.width + x] = mock_fb[r.y + y][r.x + x];
}
bool eadk_display_wait_for_vblank(void) {
  vt += 16;
  mock_frames++;
  step_ex(true);
  return true;
}
void eadk_display_draw_string(const char *t, eadk_point_t p, bool l, eadk_color_t a, eadk_color_t b) {
  (void)t, (void)p, (void)l, (void)a, (void)b;
}

static void dump(const char *name) {
  char path[256];
  snprintf(path, sizeof path, "%s/%s.ppm", snap_dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n320 240\n255\n");
  for (int y = 0; y < 240; y++)
    for (int x = 0; x < 320; x++) {
      uint16_t c = mock_fb[y][x];
      unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3 | (c >> 13)), (unsigned char)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                              (unsigned char)((c & 31) << 3 | ((c >> 2) & 7))};
      fwrite(rgb, 1, 3, f);
    }
  fclose(f);
}
void mock_snap_now(const char *name) { dump(name); }

/* Is the text-ish colour c present in the rectangle? (cheap screen checks) */
int mock_count_color(int x, int y, int w, int h, uint16_t c) {
  int n = 0;
  for (int j = y; j < y + h && j < 240; j++)
    for (int i = x; i < x + w && i < 320; i++) n += mock_fb[j][i] == c;
  return n;
}

/* ------------------------------------------------------------------ file system */
typedef struct {
  char name[64];
  uint8_t *data;
  uint32_t len;
} rec_t;
static rec_t recs[64];
static int nrecs;
#define FS_SIZE (42 * 1024)

void mock_fs_reset(void) {
  for (int i = 0; i < nrecs; i++) free(recs[i].data);
  nrecs = 0;
}
static int find(const char *name) {
  for (int i = 0; i < nrecs; i++)
    if (!strcmp(recs[i].name, name)) return i;
  return -1;
}
static uint32_t used(void) {
  uint32_t u = 0;
  for (int i = 0; i < nrecs; i++) u += 2 + (uint32_t)strlen(recs[i].name) + 1 + recs[i].len;
  return u;
}
const uint8_t *np_file_read(const char *name, uint32_t *len) {
  int i = find(name);
  if (i < 0) return 0;
  if (len) *len = recs[i].len;
  return recs[i].data;
}
bool np_file_write(const char *name, const void *data, uint32_t len) {
  int i = find(name);
  uint32_t old = i >= 0 ? 2 + (uint32_t)strlen(name) + 1 + recs[i].len : 0;
  if (used() - old + 2 + strlen(name) + 1 + len > FS_SIZE) return false;
  if (i < 0) {
    if (nrecs == 64) return false;
    i = nrecs++;
    snprintf(recs[i].name, sizeof recs[i].name, "%s", name);
    recs[i].data = 0;
  }
  uint8_t *d = malloc(len ? len : 1);
  memcpy(d, data, len);
  free(recs[i].data);
  recs[i].data = d;
  recs[i].len = len;
  return true;
}
bool np_storage_delete(const char *name) {
  int i = find(name);
  if (i < 0) return true;
  free(recs[i].data);
  recs[i] = recs[--nrecs];
  return true;
}
typedef bool (*np_record_cb_t)(const char *name, uint32_t size, void *ctx);
int np_storage_list(const char *prefix, const char *suffix, np_record_cb_t cb, void *ctx) {
  int n = 0;
  size_t pl = strlen(prefix), sl = strlen(suffix);
  for (int i = 0; i < nrecs; i++) {
    size_t rl = strlen(recs[i].name);
    if (rl < pl + sl || strncmp(recs[i].name, prefix, pl) || strcmp(recs[i].name + rl - sl, suffix)) continue;
    n++;
    char nm[64];
    strcpy(nm, recs[i].name);
    if (cb && !cb(nm, recs[i].len, ctx)) break;
  }
  return n;
}
bool np_storage_stats(uint32_t *u, uint32_t *t) {
  *u = used();
  *t = FS_SIZE;
  return true;
}

/* ------------------------------------------------------------------ launcher parts */
#if !STI_USERLAND
uint8_t np_arena[96 * 1024];
const uint32_t np_arena_size = sizeof np_arena;
#endif

#define GAME(i, t, tag) {.id = i, .title = t, .tagline = tag, .top = 0x334455, .bottom = 0x112233, .accent = 0xFF9800, .est_size = 1000}
#if !STI_USERLAND
const np_game_t np_games[] = {
    GAME("matrices", "Matrices", "L'IA"),
    GAME("snake", "Snake", "Eat the apples"),
    GAME("tetris", "Tetris", "Lines"),
    GAME("chess", "Chess", "Checkmate"),
};
const int np_game_count = 4;
#endif
int mock_last_game = -1;

bool np_game_installed(int i) { return i >= 0 && i < np_game_count; }
uint32_t np_game_size(int i) { return (uint32_t)(i + 1) * 10240; }
uint32_t np_game_ram_missing(int i) {
  (void)i;
  return 0;
}
void np_game_run(int i) { mock_last_game = i; }
void np_settings(void) {}
void np_progress_backup(void) {}
void np_progress_restore(void) {}
void np_finish_pending_uninstalls(void) {}
bool np_bg_load(int *m) {
  uint32_t n;
  const uint8_t *d = np_file_read("npbg.set", &n);
  if (!d || n != 3) return false;
  *m = d[2];
  return true;
}
bool np_bg_save(int m) {
  uint8_t d[3] = {'B', 1, (uint8_t)m};
  return np_file_write("npbg.set", d, 3);
}

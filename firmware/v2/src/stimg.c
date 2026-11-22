/* STI2D-OS V2 — images for the Gallery and the wallpaper ("stimg").
 *
 * The calculator cannot read PNG, JPEG or GIF files: its file system holds
 * about 42 KB of records in all, and only Python scripts can be sent to it
 * from a browser. So the web portal converts a picture to a small palette
 * image and wraps it in a Python script made of comments, stimg_<name>.py:
 *
 *   <0 byte>#STIMG1 <w> <h> <colours>
 *   #P <colours x 4 hex digits: RGB565>
 *   #D <base64 of the pixels, RLE: one byte = (run-1) << 4 | index>   (repeated)
 *
 * w <= 160, h <= 120, colours <= 16; the screen shows it scaled x2. A GIF is
 * exported as its first frame: an animation would not fit (docs/STIMG.md). */
#include <string.h>
#include "sti_v2_ui.h"

static int b64(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static int hexv(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static const char *read_uint(const char *p, const char *end, uint32_t *v) {
  while (p < end && *p == ' ') p++;
  if (p >= end || *p < '0' || *p > '9') return 0;
  uint32_t x = 0;
  while (p < end && *p >= '0' && *p <= '9' && x < 100000) x = x * 10 + (uint32_t)(*p++ - '0');
  *v = x;
  return p;
}

bool stimg_decode(const char *s, uint32_t n, stimg_t *img, uint8_t *out, uint32_t cap) {
  const char *end = s + n, *p = s;
  while (p < end && *p == 0) p++;
  if (end - p < 8 || strncmp(p, "#STIMG1", 7)) return false;
  p += 7;
  uint32_t w, h, nc;
  if (!(p = read_uint(p, end, &w)) || !(p = read_uint(p, end, &h)) || !(p = read_uint(p, end, &nc))) return false;
  if (!w || !h || w > STIMG_MAX_W || h > STIMG_MAX_H || nc < 1 || nc > 16 || w * h > cap) return false;
  img->w = (uint16_t)w, img->h = (uint16_t)h, img->ncolors = (uint8_t)nc;
  bool have_palette = false;
  uint32_t filled = 0, total = w * h;
  uint32_t acc = 0;
  int bits = 0;
  while (p < end) {
    while (p < end && *p != '\n') p++; /* to the next line */
    if (p >= end) break;
    p++;
    if (end - p >= 3 && p[0] == '#' && p[1] == 'P' && p[2] == ' ') {
      p += 3;
      for (uint32_t i = 0; i < nc; i++) {
        if (end - p < 4) return false;
        int v = 0;
        for (int k = 0; k < 4; k++) {
          int d = hexv(p[k]);
          if (d < 0) return false;
          v = v << 4 | d;
        }
        img->palette[i] = (color_t)v;
        p += 4;
      }
      have_palette = true;
    } else if (end - p >= 3 && p[0] == '#' && p[1] == 'D' && p[2] == ' ') {
      p += 3;
      for (; p < end && *p != '\n'; p++) {
        if (*p == '=' || *p == '\r') continue;
        int v = b64(*p);
        if (v < 0) return false;
        acc = acc << 6 | (uint32_t)v;
        bits += 6;
        if (bits >= 8) {
          bits -= 8;
          uint8_t byte = (uint8_t)(acc >> bits);
          uint32_t run = (uint32_t)(byte >> 4) + 1, idx = byte & 15u;
          if (idx >= nc || filled + run > total) return false;
          memset(out + filled, (int)idx, run);
          filled += run;
        }
      }
    }
  }
  img->px = out;
  return have_palette && filled == total;
}

bool stimg_load(const char *name, stimg_t *img) {
  if (!name || !name[0] || !ui_scratch) return false;
  uint32_t n = 0;
  const char *s = (const char *)np_file_read(name, &n);
  if (!s || !n) return false;
  return stimg_decode(s, n, img, ui_scratch, UI_SCRATCH_SIZE);
}

void stimg_draw(const stimg_t *img, int x, int y, int w, int h, int dim) {
  if (!img->px || w <= 0 || h <= 0) return;
  color_t pal[16];
  color_t bg = sti_palette()->bg;
  for (int i = 0; i < img->ncolors; i++) pal[i] = dim > 0 ? gfx_mix(img->palette[i], bg, dim >> 3) : img->palette[i];
  int y0 = NP_MAX(y, gfx_y0), y1 = NP_MIN(y + h, gfx_y1);
  int x0 = NP_MAX(x, 0), x1 = NP_MIN(x + w, SCREEN_W);
  for (int yy = y0; yy < y1; yy++) {
    const uint8_t *row = img->px + (uint32_t)((yy - y) * img->h / h) * img->w;
    color_t *dst = gfx_buf + (yy - gfx_y0) * SCREEN_W;
    for (int xx = x0; xx < x1; xx++) dst[xx] = pal[row[(xx - x) * img->w / w]];
  }
}

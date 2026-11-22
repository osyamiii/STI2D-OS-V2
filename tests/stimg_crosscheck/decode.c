/* Decodes the portal's stimg_*.py files with the firmware's own decoder (firmware/v2/src/stimg.c,
 * stimg_decode) and compares every pixel with what the portal computed (*.expect, RGB565).
 * The record is tested as the calculator stores it: a leading 0 byte (Python's import flag), then
 * the text.   decode DIR name...   (see tests/run_all.sh) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sti_v2_ui.h"

/* what stimg.c's loading and drawing functions need (not used here) */
uint8_t *ui_scratch;
color_t *gfx_buf;
int gfx_y0, gfx_y1;
const uint8_t *np_file_read(const char *name, uint32_t *len) { (void)name, (void)len; return 0; }
color_t gfx_mix(color_t a, color_t b, int t) { (void)b, (void)t; return a; }
const sti_palette_t *sti_palette(void) { static sti_palette_t p; return &p; }

static char *slurp(const char *path, long *n) {
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  *n = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *b = malloc((size_t)*n + 1);
  b[0] = 0;
  if (fread(b + 1, 1, (size_t)*n, f) != (size_t)*n) *n = -1;
  fclose(f);
  return b;
}

int main(int argc, char **argv) {
  int failed = 0;
  for (int i = 2; i < argc; i++) {
    char p[512];
    long n, m;
    snprintf(p, sizeof p, "%s/stimg_%s.py", argv[1], argv[i]);
    char *text = slurp(p, &n);
    snprintf(p, sizeof p, "%s/stimg_%s.expect", argv[1], argv[i]);
    char *exp = slurp(p, &m);
    if (!text || !exp) {
      printf("FAIL %s: missing files\n", argv[i]);
      failed++;
      continue;
    }
    static uint8_t px[STIMG_MAX_W * STIMG_MAX_H];
    stimg_t img;
    bool ok = stimg_decode(text, (uint32_t)n + 1, &img, px, sizeof px); /* +1: the flag byte */
    int bad = 0;
    if (ok) {
      const uint16_t *e = (const uint16_t *)(exp + 1);
      if (m != (long)img.w * img.h * 2) bad = -1;
      else
        for (int k = 0; k < img.w * img.h; k++) bad += img.palette[px[k]] != e[k];
    }
    if (!ok || bad) {
      printf("FAIL %s: decoded=%d differing pixels=%d\n", argv[i], ok, bad);
      failed++;
    } else {
      printf("ok   %s: %dx%d, %d colours, every pixel identical\n", argv[i], img.w, img.h, img.ncolors);
    }
    free(text);
    free(exp);
  }
  return failed ? 1 : 0;
}

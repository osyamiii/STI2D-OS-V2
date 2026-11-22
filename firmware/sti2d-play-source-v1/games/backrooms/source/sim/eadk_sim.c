// Simulateur PC minimal de l'API EADK (tests uniquement)
#include <eadk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t fb[240 * 320];
static int frame = 0, maxf = 100;
const char *eadk_external_data = 0; size_t eadk_external_data_size = 0;
void eadk_display_push_rect(eadk_rect_t r, const eadk_color_t *p) {
  for (int y = 0; y < r.height; y++) memcpy(fb + (r.y + y) * 320 + r.x, p + y * r.width, r.width * 2);
}
void eadk_display_push_rect_uniform(eadk_rect_t r, eadk_color_t c) {
  for (int y = 0; y < r.height; y++) for (int x = 0; x < r.width; x++) fb[(r.y + y) * 320 + r.x + x] = c;
}
static void dump(int f) {
  char n[256]; snprintf(n, sizeof n, "%s/f%05d.ppm", getenv("SIM_OUT"), f);
  FILE *o = fopen(n, "wb"); fprintf(o, "P6 320 240 255\n");
  for (int i = 0; i < 240 * 320; i++) { uint16_t c = fb[i]; unsigned char b[3] = {(c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3}; fwrite(b, 1, 3, o); }
  fclose(o);
}
// SIM_SCRIPT : "debut-fin:touche,touche;..."  SIM_SHOTS : "f1,f2,..."
eadk_keyboard_state_t eadk_keyboard_scan() {
  frame++;
  const char *sh = getenv("SIM_SHOTS");
  if (sh) { const char *p = sh; while (*p) { int f = atoi(p); if (f == frame) dump(frame); while (*p && *p != ',') p++; if (*p) p++; } }
  if (getenv("SIM_MAX")) maxf = atoi(getenv("SIM_MAX"));
  if (frame > maxf) exit(0);
  eadk_keyboard_state_t s = 0;
  const char *sc = getenv("SIM_SCRIPT");
  while (sc && *sc) {
    int a = atoi(sc); while (*sc != '-') sc++; sc++;
    int b = atoi(sc); while (*sc != ':') sc++; sc++;
    while (*sc && *sc != ';') { int k = atoi(sc); if (frame >= a && frame <= b) s |= (uint64_t)1 << k; while (*sc && *sc != ',' && *sc != ';') sc++; if (*sc == ',') sc++; }
    if (*sc == ';') sc++;
  }
  return s;
}
uint64_t eadk_timing_millis() { return (uint64_t)frame * 50; }
void eadk_timing_msleep(uint32_t ms) { (void)ms; }
void eadk_timing_usleep(uint32_t us) { (void)us; }
uint32_t eadk_random() { return 12345; }
bool eadk_display_wait_for_vblank() { return true; }
void eadk_display_draw_string(const char *t, eadk_point_t p, bool l, eadk_color_t a, eadk_color_t b) { (void)t; (void)p; (void)l; (void)a; (void)b; }

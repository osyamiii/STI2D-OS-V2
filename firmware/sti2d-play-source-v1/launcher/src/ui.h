#ifndef NP_UI_H
#define NP_UI_H
#include "gfx.h"

#define SHOT_W 192
#define SHOT_H 144

/* Sets up the launcher's buffers in the arena (again after each game). */
void ui_init(void);
/* A scratch buffer of the arena for STI2D-OS V2 (images of the Gallery and the
 * wallpaper: 160 x 120 palette indices), taken before the screenshot cache. */
#define UI_SCRATCH_SIZE (160 * 120)
extern uint8_t *ui_scratch;
/* Decoded screenshot (palette indices), or NULL if it cannot be decoded. */
const uint8_t *ui_shot(int game, int shot);

/* Key presses with auto-repeat for held arrows. */
typedef struct {
  uint32_t held, since, next;
} ui_keys_t;
uint32_t ui_poll(ui_keys_t *k);

float ui_ease_out(float t);
float ui_ease_in_out(float t);
float ui_approach(float value, float target, float dt, float tau);

/* A frame: renders the whole screen after the next vertical blank. */
void ui_frame(gfx_scene_t scene, void *ctx);

/* Common pieces */
void ui_button(int x, int y, int w, int h, const char *label, bool selected, uint32_t accent, int a256);
void ui_key_hint(int x, int baseline, const char *key, const char *label, color_t c);
#endif

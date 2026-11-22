/* STI2D-OS userland: the contract between the STI2D-OS shell (launcher + V2
 * layer, C) and the Epsilon userland that hosts it (apps/sti2d/host.cpp).
 *
 * In the userland build, the STI2D-OS shell is compiled into the Epsilon
 * userland (it is not an app of the external apps area). Each time Epsilon
 * shows its home screen, the host lends the shell the external apps RAM as
 * its arena and calls sti_ul_run(); the shell returns what the user chose. */
#ifndef STI_USERLAND_H
#define STI_USERLAND_H
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  STI_UL_NONE = 0,  /* nothing: show the shell again */
  STI_UL_BUILTIN,   /* an Epsilon app: index among sti_host_builtin_*() */
  STI_UL_EXTERNAL,  /* an installed app (external apps area): entry */
  STI_UL_GRID       /* the Epsilon home grid (every app, official look) */
} sti_ul_kind_t;

typedef struct {
  int kind;
  int index;
  void (*entry)(void);
} sti_ul_request_t;

/* shell side (launcher/src/main.c) */
void sti_ul_run(uint8_t *arena, uint32_t size, bool lock, bool returning, sti_ul_request_t *out);
bool sti_ul_pin_enabled(void);

/* host side (apps/sti2d/host.cpp) */
const void *sti_host_userland_header(void);
int sti_host_builtin_count(void);
const char *sti_host_builtin_name(int i); /* in the calculator's language */
bool sti_host_builtin_allowed(int i);     /* false when the exam mode forbids it */
void sti_host_demo_exam_led(bool enabled); /* visual-demo LED only; never changes Epsilon exam mode */
int sti_host_builtin_settings(void);      /* index of the Settings app, or -1 */

#ifdef __cplusplus
}
#endif
#endif

/* STI2D-OS V2 — public API of the V2 layer.
 *
 * The V2 layer runs inside the STI2D-Play launcher (an Epsilon userland app,
 * .nwa). It draws with the launcher's strip renderer (launcher/src/gfx.h) and
 * reads the keyboard through EADK. It never replaces the Epsilon kernel.
 *
 * Persistence:
 *   - sti2dos.py   preferences (environment, accent, animations, wallpaper,
 *                  date/time base). A Python script: the NumWorks installer
 *                  keeps .py records when apps are (re)installed.
 *   - sti2dos.pin  PIN record (PBKDF2-HMAC-SHA256 hashes, never the PIN).
 *                  Not a .py on purpose: it is not shown nor editable in the
 *                  Python app. Reinstalling apps from a computer erases it
 *                  (documented: that is the hard recovery path).
 *   - stical.py    calendar events.
 *   - stimg_*.py   gallery images exported by the web portal.
 */
#ifndef STI2D_V2_H
#define STI2D_V2_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STI2D_V2_VERSION_MAJOR 2
#define STI2D_V2_VERSION_MINOR 1
#define STI2D_V2_VERSION_PATCH 0
#define STI2D_V2_VERSION_STRING "2.1.0"

/* Build target, set by the Makefile (-DSTI_TARGET=...). Only informative and
 * used for per-model defaults: the binary format is the same EADK app. */
#define STI_TARGET_HOST 0
#define STI_TARGET_N0110 110
#define STI_TARGET_N0115 115
#define STI_TARGET_N0120 120
#ifndef STI_TARGET
#define STI_TARGET STI_TARGET_HOST
#endif
const char *sti_v2_target_name(void);

/* ------------------------------------------------------------ environment */
/* On the calculator there are two interfaces. The other "environments" of the
 * project (3DS, PS5, iOS, Windows, Xbox) are visual skins of the web portal
 * only, not calculator firmwares. */
typedef enum {
  STI_ENV_OS = 0,      /* STI2D-OS: honeycomb home + V2 apps */
  STI_ENV_CLASSIC = 1, /* STI2D Classic: the V1 carousel */
  STI_ENV_COUNT
} sti_env_t;

#define STI_ACCENT_COUNT 9 /* 0 = colour of the theme */
#define STI_WALL_PLAIN 0
#define STI_WALL_GRADIENT 1
#define STI_WALL_LIVE 2
#define STI_WALL_IMAGE 3
#define STI_WALL_COUNT 4
#define STI_IMG_NAME_MAX 24

typedef struct {
  uint8_t env;            /* sti_env_t */
  uint8_t accent;         /* 0..STI_ACCENT_COUNT-1 */
  uint8_t animations;     /* 0/1 */
  uint8_t wallpaper;      /* STI_WALL_* */
  char wall_image[STI_IMG_NAME_MAX]; /* record name of the wallpaper image */
  /* Wall-clock base. The calculator has no battery-backed clock reachable
   * from apps: the user sets date/time and it advances with
   * eadk_timing_millis() while the calculator stays on. */
  uint32_t date;          /* yyyymmdd, 0 = never set */
  uint32_t minute_of_day; /* at set time */
  uint32_t set_millis;    /* eadk_timing_millis() at set time */
} sti_v2_cfg_t;

void sti_v2_load(void);
bool sti_v2_save(void);
const sti_v2_cfg_t *sti_v2_cfg(void);
sti_v2_cfg_t *sti_v2_cfg_mut(void);

sti_env_t sti_v2_env(void);
void sti_v2_set_env(sti_env_t env);
const char *sti_v2_env_name(sti_env_t env);

/* ---- serialisation helpers (exposed for the host tests) */
int sti_v2_cfg_serialize(const sti_v2_cfg_t *c, char *out, int cap);
bool sti_v2_cfg_parse(sti_v2_cfg_t *c, const char *s, uint32_t n);
void sti_v2_cfg_defaults(sti_v2_cfg_t *c);

/* ---------------------------------------------------------------- date/time */
typedef struct {
  int year, month, day;   /* month 1..12 */
  int hour, minute, second;
  bool valid;             /* false: never set, or the calculator restarted */
} sti_datetime_t;
sti_datetime_t sti_v2_now(void);
void sti_v2_set_datetime(int year, int month, int day, int hour, int minute);
int sti_v2_days_in_month(int year, int month);
int sti_v2_weekday(int year, int month, int day); /* 0 = Monday */
void sti_v2_add_days(int *year, int *month, int *day, int days);

/* --------------------------------------------------------------------- PIN */
#define STI_PIN_MIN_LEN 4
#define STI_PIN_MAX_LEN 4
#define STI_PIN_FREE_ATTEMPTS 5     /* then a growing wait between attempts */
#define STI_RECOVERY_LEN 12         /* digits, shown once when the PIN is set */
#define STI_PIN_ITERATIONS 4000     /* PBKDF2-HMAC-SHA256 iterations */

bool sti_v2_pin_enabled(void);
/* Sets (or changes) the PIN. Writes a fresh random recovery code into
 * `recovery_out` (STI_RECOVERY_LEN digits + NUL) that must be shown once. */
bool sti_v2_pin_set(const char *pin, char recovery_out[STI_RECOVERY_LEN + 1]);
bool sti_v2_pin_check(const char *pin);          /* counts failures */
bool sti_v2_pin_recovery_check(const char *code); /* counts failures */
bool sti_v2_pin_disable(const char *pin);         /* needs the current PIN */
void sti_v2_pin_force_clear(void);                /* after a successful recovery */
int sti_v2_pin_failures(void);
/* Seconds to wait before the next attempt is accepted (0 = now). */
uint32_t sti_v2_pin_wait_seconds(int failures);
bool sti_v2_pin_valid_format(const char *pin);
void sti_v2_pin_reload(void);                     /* re-read the record (tests) */

/* ---- crypto (crypto.c) */
void sti_sha256(const uint8_t *data, uint32_t len, uint8_t out[32]);
void sti_hmac_sha256(const uint8_t *key, uint32_t klen, const uint8_t *msg, uint32_t mlen, uint8_t out[32]);
void sti_pbkdf2_sha256(const uint8_t *pw, uint32_t pwlen, const uint8_t *salt, uint32_t slen, uint32_t iters,
                       uint8_t out[32]);
bool sti_ct_equal(const uint8_t *a, const uint8_t *b, uint32_t n);
uint32_t sti_random32(void);

/* ---------------------------------------------------------------- double π */
/* A pure detector, fed by the home screen's key loop with the π key's press
 * edges. Only the two home screens feed it: games, apps and lists never see
 * it, and a single π press is left untouched. */
#define STI_DOUBLE_PI_WINDOW_MS 450
void sti_v2_pi_reset(void);
bool sti_v2_pi_feed(bool pi_pressed_edge, uint32_t now_ms);

/* -------------------------------------------------------------------- apps */
/* Return codes of the V2 screens (same convention as STI2D-Play's). */
enum { STI_V2_BACK = -1, STI_V2_QUIT = -2, STI_V2_SWITCH_ENV = -5 };

int sti_v2_home(bool returning);   /* a game index, STI_QUIT, STI_EXTERNAL or STI_V2_SWITCH_ENV */
int sti_v2_control_center(void);
int sti_v2_clock(void);
int sti_v2_terminal(void);
int sti_v2_gallery(void);
int sti_v2_files(void);
int sti_v2_calendar(void);
int sti_v2_pin_settings(void);
/* PIN gate at start: 0 when unlocked (or no PIN), STI_V2_QUIT to leave. */
int sti_v2_pin_unlock(void);

/* Terminal interpreter, usable without a screen (host tests). */
typedef void (*sti_term_print_t)(const char *line, void *ctx);
void sti_v2_term_exec(const char *cmd, sti_term_print_t print, void *ctx);

#ifdef __cplusplus
}
#endif
#endif

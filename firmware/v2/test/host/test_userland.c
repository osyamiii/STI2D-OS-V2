#define _GNU_SOURCE
/* STI2D-OS userland — host tests of the shell as the userland runs it.
 *
 * The real launcher and V2 sources, compiled with STI_USERLAND=1 as in
 * userland/epsilon-src/epsilon/apps/sti2d/Makefile, driven through
 * sti_ul_run() (the entry the Epsilon userland calls) with scripted keys on
 * the mock EADK (mock.c). The host side (apps/sti2d/host.cpp) is replaced by
 * the sti_host_* functions below: a list of Epsilon apps and installed apps.
 *
 * Build and run: make -C firmware/v2 test-userland */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <eadk.h>
#include "sti2d.h"
#include "sti2d_v2.h"
#include "sti_userland.h"
#include "mock.h"

static int passed, failed;
#define CHECK(c)                                                      \
  do {                                                                \
    if (c) passed++;                                                  \
    else {                                                            \
      failed++;                                                       \
      printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #c);           \
    }                                                                 \
  } while (0)

/* ------------------------------------------------------------------ host */
static const char *const builtin[] = {"Calculs", "Grapheur", "Python", "Statistiques", "Probabilites",
                                      "Inference", "Equations", "Suites", "Regressions", "Elements",
                                      "Finance", "Parametres"};
#define N_BUILTIN 12
static bool exam;
static bool demo_exam_led;
static bool demo_exam_led_was_enabled;
int sti_host_builtin_count(void) { return N_BUILTIN; }
const char *sti_host_builtin_name(int i) { return i >= 0 && i < N_BUILTIN ? builtin[i] : "?"; }
bool sti_host_builtin_allowed(int i) { return !(exam && i == 2); }
void sti_host_demo_exam_led(bool enabled) { demo_exam_led = enabled; if (enabled) demo_exam_led_was_enabled = true; }
int sti_host_builtin_settings(void) { return N_BUILTIN - 1; }
const void *sti_host_userland_header(void) { return 0; }

/* installed apps (the slot's app area) */
static void app_snake(void) {}
static void app_tetris(void) {}
static void app_matrices(void) {}
static void app_aelyx(void) {}
static void app_stmg(void) {}
static void app_personal(void) {}
static int n_apps;
int mock_apps_scan(sti_app_t *out, int max) {
  static const sti_app_t all[] = {{"Snake", app_snake, 20000}, {"Tetris", app_tetris, 30000},
                                  {"Matrices-v4", app_matrices, 90000}, {"Aelyx IA", app_aelyx, 80000},
                                  {"STMG IA", app_stmg, 75000}, {"Mes notes", app_personal, 12000}};
  int n = n_apps < max ? n_apps : max;
  for (int i = 0; i < n; i++) out[i] = all[i];
  return n;
}

/* ------------------------------------------------------------------ shell */
static uint8_t arena[150 * 1024];
static bool g_lock, g_returning;
static sti_ul_request_t req;
static int run_shell(void) {
  sti_ul_run(arena, sizeof arena, g_lock, g_returning, &req);
  return req.kind;
}
/* runs the shell over the script; returns the request, or -1 if the shell was still running */
static int shell(bool lock) {
  g_lock = lock;
  memset(&req, 0, sizeof req);
  int r = -1;
  if (mock_run(run_shell, &r)) return -1;
  return r;
}

static void fresh(void) {
  mock_fs_reset();
  sti_v2_load();
  sti_load();
  g_returning = false;
}

int main(int argc, char **argv) {
  if (argc > 1) mock_set_snap_dir(argv[1]);

  printf("userland: home screen is the shell, Back and Home do not leave it\n");
  fresh();
  n_apps = 4;
  script_reset();
  wait_ms(150);
  wait_ms(400);
  snap("ul_home");
  key(eadk_key_back);
  key(eadk_key_home);
  wait_ms(200);
  CHECK(shell(false) == -1);

  printf("userland: Calculatrice opens an Epsilon app\n");
  fresh();
  script_reset();
  wait_ms(150);
  wait_ms(300);
  key(eadk_key_ok); /* Calculatrice is the first cell */
  wait_ms(200);
  snap("ul_maths");
  key(eadk_key_down);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_BUILTIN);
  CHECK(req.index == 1);

  printf("userland: digits in Calculatrice, the Epsilon grid, exam mode\n");
  script_reset();
  wait_ms(150);
  key(eadk_key_ok);
  wait_ms(150);
  key(eadk_key_three);
  CHECK(shell(false) == STI_UL_BUILTIN && req.index == 2);
  script_reset();
  wait_ms(150);
  key(eadk_key_ok);
  wait_ms(150);
  key(eadk_key_up); /* wraps to the last row: the grid */
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_GRID);
  exam = true;
  script_reset();
  wait_ms(150);
  key(eadk_key_ok);
  wait_ms(150);
  key(eadk_key_three); /* Python, forbidden */
  wait_ms(200);
  snap("ul_exam_refused");
  CHECK(shell(false) == -1);
  exam = false;

  printf("userland: Jeux lists the installed games, not the AI apps\n");
  fresh();
  script_reset();
  wait_ms(150);
  wait_ms(200);
  key(eadk_key_right); /* Jeux */
  key(eadk_key_ok);
  wait_ms(200);
  snap("ul_games");
  key(eadk_key_down);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_EXTERNAL && req.entry == app_tetris);

  printf("userland: favourites of installed games, saved\n");
  fresh();
  script_reset();
  wait_ms(150);
  key(eadk_key_right);
  key(eadk_key_ok);
  wait_ms(150);
  key(eadk_key_shift); /* Snake */
  key(eadk_key_back);
  key(eadk_key_right); /* Favoris */
  key(eadk_key_ok);
  wait_ms(200);
  snap("ul_favorites");
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_EXTERNAL && req.entry == app_snake);
  {
    uint32_t n = 0;
    const uint8_t *d = np_file_read("sti2dplay.py", &n);
    CHECK(d && memmem(d, n, "A=\"", 3));
  }
  sti_load(); /* the favourite comes back from the file */
  CHECK(sti_count_games(true) == 1);
  CHECK(sti_count_games(false) == 2);

  printf("userland: My Apps contains only personally injected apps\n");
  fresh();
  n_apps = 6;
  script_reset();
  wait_ms(150);
  key(eadk_key_down); /* My Apps, directly below the first cell */
  key(eadk_key_ok);
  wait_ms(180);
  snap("ul_my_apps");
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_EXTERNAL && req.entry == app_personal);

  printf("userland: Intelligences contains all three IA choices\n");
  fresh();
  n_apps = 3;
  script_reset();
  wait_ms(150);
  key_n(eadk_key_right, 3);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_EXTERNAL && req.entry == app_matrices);
  n_apps = 6;
  script_reset();
  wait_ms(150);
  key_n(eadk_key_right, 3);
  key(eadk_key_ok);
  wait_ms(200);
  snap("ul_ai_list");
  key_n(eadk_key_down, 2);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_EXTERNAL && req.entry == app_stmg);
  n_apps = 2;
  script_reset();
  wait_ms(150);
  key_n(eadk_key_right, 3);
  key(eadk_key_ok);
  wait_ms(200);
  snap("ul_ai_none");
  CHECK(shell(false) == -1);

  printf("userland: Control Center opens the NumWorks settings\n");
  fresh();
  n_apps = 4;
  script_reset();
  wait_ms(150);
  key_n(eadk_key_down, 2); /* row 2: Terminal */
  key(eadk_key_right); /* Centre de controle */
  key(eadk_key_ok);
  wait_ms(150);
  key_n(eadk_key_down, 9); /* Parametres NumWorks */
  wait_ms(150);
  snap("ul_control_center");
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_BUILTIN && req.index == N_BUILTIN - 1);

  printf("userland: double pi to STI2D Classic, its Calculatrice card, and back\n");
  fresh();
  script_reset();
  wait_ms(150);
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(400);
  snap("ul_classic");
  key(eadk_key_ok); /* Calculatrice card */
  wait_ms(150);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_BUILTIN && req.index == 0);
  CHECK(sti_v2_env() == STI_ENV_CLASSIC);
  script_reset();
  wait_ms(150);
  key(eadk_key_back); /* nothing to quit to */
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(300);
  snap("ul_back_to_os");
  CHECK(shell(false) == -1);
  CHECK(sti_v2_env() == STI_ENV_OS);

  printf("userland: Files reads a transferred text document without executing it\n");
  fresh();
  {
    static const char document[] = "# coding: utf-8\n# STI2D-DOC-V1\n# TITLE: Document de test\n#|Premiere ligne du document\n#|Deuxieme ligne\n";
    CHECK(np_file_write("stidoc_test.py", document, sizeof document - 1));
  }
  script_reset();
  wait_ms(150);
  key(eadk_key_down); /* Mes apps */
  key(eadk_key_down); /* Galerie */
  key_n(eadk_key_right, 3); /* Fichiers */
  key(eadk_key_ok);
  wait_ms(120);
  key(eadk_key_ok); /* open the transferred text file */
  wait_ms(180);
  snap("ul_file_reader");
  key(eadk_key_back); /* back to the files list */
  wait_ms(180);
  CHECK(shell(false) == -1);

  printf("userland: Classic exam mode is a non-restrictive demo with LED cleanup\n");
  fresh();
  demo_exam_led = false;
  demo_exam_led_was_enabled = false;
  script_reset();
  wait_ms(150);
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(350);
  key_n(eadk_key_right, 5); /* Mode examen */
  key(eadk_key_ok);
  wait_ms(900);
  snap("ul_classic_exam_demo");
  key(eadk_key_back);
  wait_ms(100);
  key(eadk_key_pi);
  key(eadk_key_pi);
  wait_ms(300);
  CHECK(shell(false) == -1);
  CHECK(demo_exam_led_was_enabled);
  CHECK(!demo_exam_led);
  CHECK(sti_v2_env() == STI_ENV_OS);

  printf("userland: lock screen cannot be skipped\n");
  fresh();
  char rec[STI_RECOVERY_LEN + 1];
  CHECK(sti_v2_pin_set("2580", rec));
  CHECK(sti_ul_pin_enabled());
  script_reset();
  wait_ms(150);
  wait_ms(200);
  snap("ul_locked");
  key(eadk_key_back);
  key(eadk_key_home);
  wait_ms(200);
  CHECK(shell(true) == -1);
  script_reset();
  wait_ms(150);
  key(eadk_key_two);
  key(eadk_key_five);
  key(eadk_key_eight);
  key(eadk_key_zero);
  key(eadk_key_ok);
  wait_ms(9000); /* PBKDF2 */
  snap("ul_unlocked");
  key(eadk_key_ok); /* then Calculatrice > Calculs */
  wait_ms(150);
  key(eadk_key_ok);
  CHECK(shell(true) == STI_UL_BUILTIN && req.index == 0);
  /* not locked again when coming back from an app */
  script_reset();
  wait_ms(150);
  key(eadk_key_ok);
  wait_ms(150);
  key(eadk_key_ok);
  CHECK(shell(false) == STI_UL_BUILTIN);

  printf("\n%d checks passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}

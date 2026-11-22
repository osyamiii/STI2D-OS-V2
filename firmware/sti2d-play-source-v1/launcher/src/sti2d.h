/* STI2D-Play : l'interface (accueil, listes, reglages) par-dessus le moteur de NumPlay. */
#ifndef STI2D_H
#define STI2D_H
#include "np.h"

/* Apps installees a cote de STI2D-Play (gros jeux, emulateurs) : lues dans la zone des apps. */
typedef struct {
  const char *name;
  void (*entry)(void);
  uint32_t size;
} sti_app_t;
#define STI_MAX_APPS 32
int sti_apps_scan(sti_app_t *out, int max);
/* Remet a neuf la RAM de l'app (apres une app lancee, ou np_session_end) */
void sti_reinit_ram(void);

/* Ce que l'accueil demande a main() */
enum { STI_QUIT = -2, STI_EXTERNAL = -3, STI_GRID = -4 };
/* STI2D-OS userland: an Epsilon app to open, STI_BUILTIN_BASE - index */
#define STI_BUILTIN_BASE (-100)
extern void (*sti_external)(void);   /* l'app a lancer quand sti_home() rend STI_EXTERNAL */

void sti_load(void);                  /* reglages et favoris (sti2dplay.py) */
/* un jeu de np_games a lancer, STI_QUIT ou STI_EXTERNAL. Ouvre l'accueil de
 * l'interface choisie : STI2D-OS (nid d'abeilles, V2) ou STI2D Classic. */
int sti_home(bool returning);

/* ---- partage avec la couche STI2D-OS V2 (firmware/v2) : un seul theme */
typedef struct {
  color_t bg, panel, line, text, dim, acc, acc2, dark;
  uint32_t bg888, top888, bottom888, acc888;
  bool light;
} sti_palette_t;
const sti_palette_t *sti_palette(void);
void sti_apply_theme(void);           /* apres un changement de theme, de mode ou d'accent */
int sti_theme_count(void);
int sti_theme_get(void);
void sti_theme_set(int theme);        /* enregistre */
const char *sti_theme_name(int theme);
bool sti_light_get(void);
void sti_light_set(bool light);       /* enregistre */
/* les ecrans de STI2D-Play, ouverts aussi depuis l'accueil STI2D-OS :
 * un jeu, -1 (retour), STI_QUIT ou STI_EXTERNAL */
int sti_run_list(bool apps, bool favs);
int sti_run_settings(void);
int sti_run_credits(void);
int sti_matrices_game(void);          /* l'IA Matrices, ou -1 */
int sti_count_games(bool favs);
int sti_count_apps(void);             /* apps installed in the app area */
#endif

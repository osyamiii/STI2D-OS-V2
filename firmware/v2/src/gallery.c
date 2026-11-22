/* STI2D-OS V2 — Gallery: the images on the calculator (stimg_*.py records,
 * made by the web portal: see stimg.c). List, preview, full screen, delete,
 * set as wallpaper. Importing happens from a computer or phone: the portal
 * exports the image as a Python script, sent like any script (the screen
 * explains how when the gallery is empty or on Toolbox). */
#include <string.h>
#include "sti_v2_ui.h"

#define MAX_IMAGES 24
#define LIST_W 128
#define ROW_H 22

typedef struct {
  char names[MAX_IMAGES][STI_IMG_NAME_MAX];
  uint32_t sizes[MAX_IMAGES];
  int n, sel, top;
  stimg_t img;
  bool img_ok;
  int img_for; /* index decoded into img, -1 */
  bool full;
} gal_t;

static gal_t G;

static bool collect(const char *name, uint32_t size, void *ctx) {
  (void)ctx;
  if (G.n >= MAX_IMAGES || strlen(name) >= STI_IMG_NAME_MAX) return G.n < MAX_IMAGES;
  strcpy(G.names[G.n], name);
  G.sizes[G.n] = size;
  G.n++;
  return true;
}

static void scan(void) {
  G.n = 0;
  np_storage_list("stimg_", ".py", collect, 0);
  if (G.sel >= G.n) G.sel = G.n ? G.n - 1 : 0;
  G.img_for = -1;
}

/* "stimg_my_cat.py" -> "my cat" */
static void pretty(const char *rec, char *out, int cap) {
  const char *s = rec + 6;
  int n = 0;
  while (*s && n < cap - 1) {
    if (!strcmp(s, ".py")) break;
    out[n++] = *s == '_' ? ' ' : *s;
    s++;
  }
  out[n] = 0;
}

static void decode_selected(void) {
  if (G.img_for == G.sel) return;
  G.img_ok = G.n && stimg_load(G.names[G.sel], &G.img);
  G.img_for = G.sel;
  v2_wallpaper_invalidate(); /* the scratch buffer now holds this image */
}

static void gal_scene(void *ctx) {
  (void)ctx;
  const sti_palette_t *p = sti_palette();
  if (G.full && G.img_ok) {
    stimg_draw(&G.img, 0, 0, SCREEN_W, SCREEN_H, 0);
    if (gfx_y1 > 214) {
      gfx_fill_alpha(0, 214, SCREEN_W, 26, 0, 170);
      gfx_text_center(&np_font_small, SCREEN_W / 2, 231, "Shift fond d'ecran   <- supprimer   Back", 0xFFFF, 256);
    }
    return;
  }
  v2_background();
  char right[16];
  v2_put(v2_put_uint(right, (uint32_t)G.n), G.n == 1 ? " image" : " images");
  v2_header("Galerie", right);
  if (!G.n) {
    v2_glyph(V2_GLYPH_GALLERY, SCREEN_W / 2, 66, p->dim);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 100, "Aucune image sur la calculatrice", p->text, 256);
    gfx_paragraph(&np_font_small, SCREEN_W / 2, 122, SCREEN_W - 40, 13,
                  "Sur le portail STI2D-OS : Fonds d'ecran > importe une image > Exporter pour la "
                  "calculatrice. Envoie ensuite le fichier stimg_*.py comme un script Python "
                  "(my.numworks.com, ou nwlink). Il apparaitra ici.",
                  p->dim, 256);
    v2_footer("Back retour");
    return;
  }
  const char *wall = sti_v2_cfg()->wall_image;
  int rows = (V2_FOOT_Y - V2_HEAD_H - 8) / ROW_H;
  for (int i = 0; i < rows; i++) {
    int k = G.top + i;
    if (k >= G.n) break;
    int y = V2_HEAD_H + 4 + i * ROW_H;
    if (y >= gfx_y1 || y + ROW_H <= gfx_y0) continue;
    bool sel = k == G.sel;
    if (sel) gfx_rrect(4, y + 1, LIST_W - 4, ROW_H - 2, 7, p->acc, 256);
    char name[STI_IMG_NAME_MAX];
    pretty(G.names[k], name, sizeof name);
    gfx_text(&np_font_body, 12, y + 15, name, sel ? p->dark : p->text, 256);
    if (!strcmp(G.names[k], wall) && sti_v2_cfg()->wallpaper == STI_WALL_IMAGE)
      gfx_circle(LIST_W - 10, y + ROW_H / 2, 3, sel ? p->dark : p->acc, 256);
  }
  int x = LIST_W + 8, w = SCREEN_W - x - 8, ih = w * 3 / 4, y = V2_HEAD_H + 8;
  if (G.img_ok) {
    stimg_draw(&G.img, x, y, w, ih, 0);
    char info[32], *o = v2_put_uint(info, G.img.w);
    o = v2_put(o, " x ");
    o = v2_put_uint(o, G.img.h);
    o = v2_put(o, "  -  ");
    o = v2_put_uint(o, (G.sizes[G.sel] + 1023) / 1024);
    v2_put(o, " Ko");
    gfx_text_center(&np_font_small, x + w / 2, y + ih + 16, info, p->dim, 256);
  } else {
    gfx_rrect(x, y, w, ih, 8, p->panel, 256);
    gfx_text_center(&np_font_small, x + w / 2, y + ih / 2 + 4, "Image illisible", RGB(240, 80, 70), 256);
  }
  gfx_text(&np_font_small, x, y + ih + 38, "OK plein ecran", p->dim, 256);
  gfx_text(&np_font_small, x, y + ih + 52, "Shift fond d'ecran", p->dim, 256);
  gfx_text(&np_font_small, x, y + ih + 66, "<- supprimer", p->dim, 256);
  gfx_text(&np_font_small, x, y + ih + 80, "Toolbox importer", p->dim, 256);
  v2_footer("Haut/Bas choisir   Back retour");
}

static void import_help(void) {
  v2_message("Importer une image",
             "Portail STI2D-OS > Fonds d'ecran : choisis une image (PNG, JPEG, GIF), puis "
             "\"Exporter pour la calculatrice\".",
             "Envoie le fichier stimg_*.py : my.numworks.com > Python, ou npx nwlink. 160 x 120 max, 16 couleurs.");
}

int sti_v2_gallery(void) {
  memset(&G, 0, sizeof G);
  G.img_for = -1;
  scan();
  v2_keys_t k;
  v2_keys_init(&k);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&k);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) {
      if (G.full) G.full = false, dirty = true;
      else return STI_V2_BACK;
    }
    if (p & V2_KB(eadk_key_toolbox)) {
      import_help();
      v2_keys_init(&k);
      dirty = true;
    }
    if (G.n) {
      if (!G.full && (p & V2_KB(eadk_key_up))) G.sel = (G.sel + G.n - 1) % G.n, dirty = true;
      if (!G.full && (p & V2_KB(eadk_key_down))) G.sel = (G.sel + 1) % G.n, dirty = true;
      if (G.full && (p & (V2_KB(eadk_key_left) | V2_KB(eadk_key_right)))) {
        G.sel = (G.sel + ((p & V2_KB(eadk_key_left)) ? G.n - 1 : 1)) % G.n;
        dirty = true;
      }
      if (v2_is_ok(p) && G.img_ok) G.full = !G.full, dirty = true;
      if (p & V2_KB(eadk_key_shift)) {
        if (G.img_ok) {
          sti_v2_cfg_t *c = sti_v2_cfg_mut();
          strcpy(c->wall_image, G.names[G.sel]);
          c->wallpaper = STI_WALL_IMAGE;
          sti_v2_save();
          v2_message("Fond d'ecran", "Cette image est maintenant le fond de l'accueil STI2D-OS.", 0);
          v2_keys_init(&k);
        }
        dirty = true;
      }
      if (p & V2_KB(eadk_key_backspace)) {
        char name[STI_IMG_NAME_MAX], l1[48];
        pretty(G.names[G.sel], name, sizeof name);
        v2_put(v2_put(v2_put(l1, "Supprimer \""), name), "\" de la calculatrice ?");
        if (v2_confirm("Supprimer l'image", l1, "Cette action est definitive.")) {
          sti_v2_cfg_t *c = sti_v2_cfg_mut();
          if (!strcmp(c->wall_image, G.names[G.sel])) {
            c->wall_image[0] = 0;
            if (c->wallpaper == STI_WALL_IMAGE) c->wallpaper = STI_WALL_GRADIENT;
            sti_v2_save();
          }
          if (!np_storage_delete(G.names[G.sel])) v2_message("Echec", "L'image n'a pas pu etre supprimee.", 0);
          G.full = false;
          scan();
        }
        v2_keys_init(&k);
        dirty = true;
      }
      int rows = (V2_FOOT_Y - V2_HEAD_H - 8) / ROW_H;
      if (G.sel < G.top) G.top = G.sel;
      if (G.sel >= G.top + rows) G.top = G.sel - rows + 1;
    }
    if (dirty) {
      if (G.n) decode_selected();
      ui_frame(gal_scene, 0);
      dirty = false;
    } else {
      np_sleep(12);
    }
  }
}

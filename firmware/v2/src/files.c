/* STI2D-OS V2 — lightweight text document reader.
 *
 * The portal exports a .py script containing comments only:
 *   # STI2D-DOC-V1
 *   # TITLE: My document
 *   #|first line of text
 *   #|second line
 *
 * Such a script is transferred to the calculator like any normal Python file.
 * The reader never executes it; it only extracts the comment records.
 */
#include <string.h>
#include "sti_v2_ui.h"

#define DOC_PREFIX "stidoc_"
#define DOC_SUFFIX ".py"
#define MAX_DOCS 24
#define DOC_NAME_MAX 64
#define DOC_TEXT_MAX (UI_SCRATCH_SIZE - 1)
#define DOC_VISUAL_LINES 240
#define DOC_CHARS_PER_LINE 43
#define DOC_ROWS 11
#define DOC_ROW_H 13

typedef struct {
  char names[MAX_DOCS][DOC_NAME_MAX];
  uint32_t sizes[MAX_DOCS];
  int count, selected, top;
  bool reading;
  char title[40];
  uint16_t offsets[DOC_VISUAL_LINES];
  uint8_t lengths[DOC_VISUAL_LINES];
  int line_count, scroll;
  bool truncated;
} files_t;

static files_t F;

static bool collect_doc(const char *name, uint32_t size, void *ctx) {
  (void)ctx;
  if (F.count >= MAX_DOCS || strlen(name) >= DOC_NAME_MAX) return F.count < MAX_DOCS;
  strcpy(F.names[F.count], name);
  F.sizes[F.count] = size;
  F.count++;
  return true;
}

static void scan_docs(void) {
  F.count = 0;
  np_storage_list(DOC_PREFIX, DOC_SUFFIX, collect_doc, 0);
  if (F.selected >= F.count) F.selected = F.count ? F.count - 1 : 0;
  F.top = 0;
}

static void doc_title_from_name(const char *name, char *out, int cap) {
  const char *s = name + strlen(DOC_PREFIX);
  int n = 0;
  while (*s && n < cap - 1) {
    if (s[0] == '.' && !strcmp(s, DOC_SUFFIX)) break;
    out[n++] = *s == '_' || *s == '-' ? ' ' : *s;
    s++;
  }
  out[n] = 0;
  if (!n) strcpy(out, "Document");
}

static bool load_doc(int index) {
  if (!ui_scratch || index < 0 || index >= F.count) return false;
  uint32_t len = 0;
  const uint8_t *raw = np_file_read(F.names[index], &len);
  if (!raw || len == 0 || len > UI_SCRATCH_SIZE * 2) return false;

  char *out = (char *)ui_scratch;
  uint32_t used = 0;
  bool title_seen = false, body = false, copy_truncated = false;
  uint32_t at = 0;
  F.title[0] = 0;
  while (at < len && used < DOC_TEXT_MAX - 1) {
    uint32_t start = at;
    while (at < len && raw[at] != '\n') at++;
    uint32_t n = at - start;
    if (at < len && raw[at] == '\n') at++;
    if (n && raw[start + n - 1] == '\r') n--;

    if (!title_seen && n >= 9 && !memcmp(raw + start, "# TITLE: ", 9)) {
      uint32_t take = n - 9;
      if (take > sizeof F.title - 1) take = sizeof F.title - 1;
      memcpy(F.title, raw + start + 9, take);
      F.title[take] = 0;
      title_seen = true;
      continue;
    }
    if (n >= 14 && !memcmp(raw + start, "# STI2D-DOC-V1", 14)) {
      body = true;
      continue;
    }
    if (n >= 2 && raw[start] == '#' && raw[start + 1] == '|') {
      body = true;
      uint32_t take = n - 2;
      if (take > DOC_TEXT_MAX - used - 2) take = DOC_TEXT_MAX - used - 2;
      memcpy(out + used, raw + start + 2, take);
      used += take;
      out[used++] = '\n';
      if (take < n - 2) copy_truncated = true;
    } else if (body && n == 0) {
      /* Empty exported text lines are encoded as '#|' and handled above. */
    }
  }
  out[used] = 0;
  if (!F.title[0]) doc_title_from_name(F.names[index], F.title, sizeof F.title);
  F.line_count = 0;
  F.scroll = 0;
  F.truncated = copy_truncated;

  /* Index visible rows without copying the text a second time. Wrap at UTF-8
     code-point boundaries; the rendering font is small enough for 43 glyphs. */
  uint32_t pos = 0;
  while (pos < used && F.line_count < DOC_VISUAL_LINES) {
    uint32_t end = pos;
    int chars = 0;
    uint32_t last_space = pos;
    while (end < used && out[end] != '\n' && chars < DOC_CHARS_PER_LINE) {
      unsigned char c = (unsigned char)out[end];
      uint32_t bytes = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
      if (end + bytes > used || out[end + bytes - 1] == '\n') bytes = 1;
      if (c == ' ') last_space = end;
      end += bytes;
      chars++;
    }
    uint32_t line_end = end;
    if (end < used && out[end] != '\n' && last_space > pos) line_end = last_space;
    if (line_end == pos && end > pos) line_end = end;
    F.offsets[F.line_count] = (uint16_t)pos;
    F.lengths[F.line_count] = (uint8_t)(line_end - pos);
    F.line_count++;
    pos = line_end;
    while (pos < used && (out[pos] == ' ' || out[pos] == '\n')) {
      if (out[pos] == '\n') { pos++; break; }
      pos++;
    }
  }
  if (pos < used) F.truncated = true;
  v2_wallpaper_invalidate();
  return true;
}

static void files_list_scene(void *ctx) {
  (void)ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  char right[20], *o = v2_put_uint(right, (uint32_t)F.count);
  v2_put(o, F.count == 1 ? " document" : " documents");
  v2_header("Fichiers", right);
  if (!F.count) {
    v2_glyph(V2_GLYPH_FILES, SCREEN_W / 2, 72, p->dim);
    gfx_text_center(&np_font_body, SCREEN_W / 2, 106, "Aucun document", p->text, 256);
    gfx_paragraph(&np_font_small, SCREEN_W / 2, 130, SCREEN_W - 36, 13,
      "Sur le portail STI2D-OS > Lecteur de fichiers, importe un fichier TXT ou Markdown et exporte le script stidoc_*.py. Transfere-le ensuite sur la calculatrice comme un script Python.", p->dim, 256);
    v2_footer("Back retour");
    return;
  }
  for (int i = 0; i < 7; i++) {
    int k = F.top + i;
    if (k >= F.count) break;
    int y = V2_HEAD_H + 6 + i * 25;
    bool sel = k == F.selected;
    if (sel) gfx_rrect(8, y, SCREEN_W - 16, 22, 6, p->acc, 256);
    char name[44];
    doc_title_from_name(F.names[k], name, sizeof name);
    gfx_text(&np_font_body, 16, y + 15, name, sel ? p->dark : p->text, 256);
    char size_txt[14], *s = v2_put_uint(size_txt, (F.sizes[k] + 1023) / 1024);
    v2_put(s, " Ko");
    gfx_text_right(&np_font_small, SCREEN_W - 16, y + 14, size_txt, sel ? p->dark : p->dim, 256);
  }
  v2_footer("OK ouvrir   <- supprimer   Back retour");
}

static void files_reader_scene(void *ctx) {
  (void)ctx;
  const sti_palette_t *p = sti_palette();
  v2_background();
  v2_header(F.title, "TXT");
  if (!ui_scratch) {
    gfx_text_center(&np_font_body, SCREEN_W / 2, 110, "Memoire indisponible", p->text, 256);
    v2_footer("Back retour");
    return;
  }
  const char *text = (const char *)ui_scratch;
  for (int row = 0; row < DOC_ROWS; row++) {
    int index = F.scroll + row;
    if (index >= F.line_count) break;
    char line[DOC_TEXT_MAX > 1 ? DOC_CHARS_PER_LINE * 4 + 1 : 2];
    int n = F.lengths[index];
    if (n > (int)sizeof line - 1) n = sizeof line - 1;
    memcpy(line, text + F.offsets[index], (size_t)n);
    line[n] = 0;
    gfx_text(&np_font_small, 10, 52 + row * DOC_ROW_H, line, p->text, 256);
  }
  char page[24], *o = v2_put_uint(page, (uint32_t)(F.scroll / DOC_ROWS + 1));
  o = v2_put(o, "/");
  v2_put_uint(o, (uint32_t)((F.line_count + DOC_ROWS - 1) / DOC_ROWS));
  if (gfx_y1 > 205) gfx_text_right(&np_font_small, SCREEN_W - 10, 210, page, p->dim, 256);
  v2_footer(F.truncated ? "Haut/Bas defiler   Back liste (texte tronque)" : "Haut/Bas defiler   Back liste");
}

int sti_v2_files(void) {
  memset(&F, 0, sizeof F);
  scan_docs();
  v2_keys_t keys;
  v2_keys_init(&keys);
  bool dirty = true;
  for (;;) {
    uint64_t p = v2_poll(&keys);
    if (v2_is_home(p)) return STI_V2_QUIT;
    if (v2_is_back(p)) {
      if (F.reading) F.reading = false, dirty = true;
      else return STI_V2_BACK;
    }
    if (!F.reading && F.count) {
      if (p & V2_KB(eadk_key_up)) F.selected = (F.selected + F.count - 1) % F.count, dirty = true;
      if (p & V2_KB(eadk_key_down)) F.selected = (F.selected + 1) % F.count, dirty = true;
      if (F.selected < F.top) F.top = F.selected;
      if (F.selected >= F.top + 7) F.top = F.selected - 6;
      if (v2_is_ok(p)) {
        if (load_doc(F.selected)) F.reading = true;
        else v2_message("Document illisible", "Le fichier est absent, invalide ou trop volumineux.", 0);
        v2_keys_init(&keys);
        dirty = true;
      }
      if (p & V2_KB(eadk_key_backspace)) {
        char line[80], name[44];
        doc_title_from_name(F.names[F.selected], name, sizeof name);
        v2_put(v2_put(v2_put(line, "Supprimer "), name), " ?");
        if (v2_confirm("Supprimer le document", line, "Le script Python sera retire de la calculatrice.")) {
          np_storage_delete(F.names[F.selected]);
          scan_docs();
        }
        v2_keys_init(&keys);
        dirty = true;
      }
    } else if (F.reading) {
      int max_scroll = F.line_count > DOC_ROWS ? F.line_count - DOC_ROWS : 0;
      if (p & V2_KB(eadk_key_down)) F.scroll = F.scroll < max_scroll ? F.scroll + 1 : max_scroll, dirty = true;
      if (p & V2_KB(eadk_key_up)) F.scroll = F.scroll > 0 ? F.scroll - 1 : 0, dirty = true;
      if (p & V2_KB(eadk_key_right)) F.scroll = F.scroll + DOC_ROWS < max_scroll ? F.scroll + DOC_ROWS : max_scroll, dirty = true;
      if (p & V2_KB(eadk_key_left)) F.scroll = F.scroll > DOC_ROWS ? F.scroll - DOC_ROWS : 0, dirty = true;
    }
    if (dirty) {
      ui_frame(F.reading ? files_reader_scene : files_list_scene, 0);
      dirty = false;
    } else np_sleep(12);
  }
}

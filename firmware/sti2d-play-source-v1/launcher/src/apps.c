/* Les apps installees a cote de STI2D-Play, parcourues comme le fait l'ecran
 * d'accueil d'Epsilon : chacune commence par 0xDEC0BEBA, sa taille donne la
 * suivante, alignee sur 64 Ko. On lance une app comme Epsilon : en appelant
 * son point d'entree, qui recopie ses donnees dans la meme RAM que la notre. */
#include <string.h>
#include "sti2d.h"
#include "sys.h"

#define APP_MAGIC 0xDEC0BEBAu
#define APP_SECTOR 0x10000u

#if PLATFORM_DEVICE && !NP_SIMULATOR
extern const char eadk_app_name[];

int sti_apps_scan(sti_app_t *out, int max) {
  const np_userland_t *h = np_userland();
  if (!h) return 0;
  uint32_t a = h->apps_flash_start, end = h->apps_flash_end;
  if (a < 0x90000000u || end > 0x91000000u || end <= a) return 0;
  int n = 0;
  while (n < max && a + 32 <= end) {
    const volatile uint32_t *info = (const volatile uint32_t *)a;
    if (info[0] != APP_MAGIC || info[7] != APP_MAGIC) break;
    uint32_t size = info[6];
    if (size < 32 || a + size > end) break;
    /* API level 0 ; pas STI2D-Play lui-meme */
    if (info[1] == 0 && info[2] < size && info[5] < size) {
      const char *name = (const char *)(a + info[2]);
      if (strcmp(name, eadk_app_name) != 0) {
        out[n].name = name;
        out[n].entry = (void (*)(void))((a + info[5]) | 1);
        out[n].size = size;
        n++;
      }
    }
    a = (a + size + APP_SECTOR - 1) & ~(APP_SECTOR - 1);
  }
  return n;
}

#if STI_USERLAND
/* the userland's RAM is the userland's: the shell restarts from its host */
void sti_reinit_ram(void) {}
#else
extern char _data_section_start_flash[], _data_section_start_ram[], _data_section_end_ram[];
extern char _bss_section_start_ram[], _bss_section_end_ram[];

void sti_reinit_ram(void) {
  volatile char *d = _data_section_start_ram;
  const char *s = _data_section_start_flash;
  while (d < _data_section_end_ram) *d++ = *s++;
  for (d = _bss_section_start_ram; d < _bss_section_end_ram; d++) *d = 0;
}
#endif
#elif STI_USERLAND
/* host tests of the userland shell: the installed apps come from the test */
int mock_apps_scan(sti_app_t *out, int max);
int sti_apps_scan(sti_app_t *out, int max) { return mock_apps_scan(out, max); }
void sti_reinit_ram(void) {}
#else
int sti_apps_scan(sti_app_t *out, int max) {
  (void)out, (void)max;
  return 0;
}
void sti_reinit_ram(void) {}
#endif

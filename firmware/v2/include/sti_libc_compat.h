/* STI2D-OS userland: the few C library functions the shell uses that Epsilon's
 * own C library (liba) does not have. Force-included in the userland build
 * only (apps/sti2d/Makefile); the .nwa build uses newlib's. */
#ifndef STI_LIBC_COMPAT_H
#define STI_LIBC_COMPAT_H
#include <stddef.h>
#include <string.h>

static inline char *sti_strcpy(char *d, const char *s) {
  char *o = d;
  while ((*d++ = *s++)) {
  }
  return o;
}
static inline char *sti_strncpy(char *d, const char *s, size_t n) {
  size_t i = 0;
  for (; i < n && s[i]; i++) d[i] = s[i];
  for (; i < n; i++) d[i] = 0;
  return d;
}
static inline char *sti_strcat(char *d, const char *s) {
  sti_strcpy(d + strlen(d), s);
  return d;
}
static inline char *sti_strncat(char *d, const char *s, size_t n) {
  char *e = d + strlen(d);
  size_t i = 0;
  for (; i < n && s[i]; i++) e[i] = s[i];
  e[i] = 0;
  return d;
}
static inline size_t sti_strnlen(const char *s, size_t n) {
  size_t i = 0;
  while (i < n && s[i]) i++;
  return i;
}
static inline char *sti_strstr(const char *h, const char *n) {
  size_t k = strlen(n);
  if (!k) return (char *)h;
  for (; *h; h++)
    if (!strncmp(h, n, k)) return (char *)h;
  return NULL;
}
#define strcpy sti_strcpy
#define strncpy sti_strncpy
#define strcat sti_strcat
#define strncat sti_strncat
#define strnlen sti_strnlen
#define strstr sti_strstr
#endif

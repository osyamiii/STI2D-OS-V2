// Petites fonctions de chaines (pas de dependance a la libc de la cible)
#ifndef MINI_H
#define MINI_H
#include <stddef.h>
#define NOPAT __attribute__((optimize("no-tree-loop-distribute-patterns")))
static NOPAT size_t m_strlen(const char *s){ size_t n=0; while(s[n]) n++; return n; }
static NOPAT char *m_strcpy(char *d,const char *s){ char *r=d; while((*d++=*s++)); return r; }
static NOPAT char *m_strcat(char *d,const char *s){ char *r=d; while(*d) d++; while((*d++=*s++)); return r; }
static NOPAT char *m_strncpy(char *d,const char *s,size_t n){ size_t i=0; for(;i<n&&s[i];i++) d[i]=s[i]; for(;i<n;i++) d[i]=0; return d; }
static NOPAT int m_strcmp(const char *a,const char *b){ while(*a&&*a==*b){ a++; b++; } return (unsigned char)*a-(unsigned char)*b; }
static NOPAT int m_strncmp(const char *a,const char *b,size_t n){ for(;n;n--,a++,b++){ if(*a!=*b) return (unsigned char)*a-(unsigned char)*b; if(!*a) return 0; } return 0; }
static NOPAT int m_memcmp(const void *a,const void *b,size_t n){ const unsigned char *x=a,*y=b; for(;n;n--,x++,y++) if(*x!=*y) return *x-*y; return 0; }
static NOPAT void *m_memmove(void *d,const void *s,size_t n){ unsigned char *x=d; const unsigned char *y=s;
  if(x<y){ for(size_t i=0;i<n;i++) x[i]=y[i]; } else { for(size_t i=n;i>0;i--) x[i-1]=y[i-1]; } return d; }
#endif

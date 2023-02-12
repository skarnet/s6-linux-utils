/* ISC license. */

#include <string.h>
#include <sys/utsname.h>

#include <skalibs/uint64.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>

#include "s6ps.h"

int s6ps_wchan_init (s6ps_wchan_t *w, char const *file)
{
  if (file)
  {
    if (!openslurpclose(&w->sysmap, file)) return 0 ;
  }
  else
  {
    char *files[3] = { "/proc/kallsyms", 0, "/boot/System.map" } ;
    struct utsname uts ;
    size_t n ;
    if (uname(&uts) < 0) return 0 ;
    n = strlen(uts.release) ;
    {
      char buf[18 + n] ;
      unsigned int i = 0 ;
      memcpy(buf, "/boot/System.map", 16) ;
      buf[16] = '-' ;
      memcpy(buf + 17, uts.release, n + 1) ;
      files[1] = buf ;
      for (; i < 3 ; i++)
        if (openslurpclose(&w->sysmap, files[i])) break ;
      if (i >= 3) return 0 ;
    }
  }
  {
    size_t i = 0 ;
    if (!genalloc_append(size_t, &w->ind, &i)) goto err2 ;
    for (i = 1 ; i <= w->sysmap.len ; i++)
      if (w->sysmap.s[i-1] == '\n')
        if (!genalloc_append(size_t, &w->ind, &i)) goto err ;
  }
  return 1 ;
 err:
  genalloc_free(size_t, &w->ind) ;
 err2:
  stralloc_free(&w->sysmap) ;
  return 0 ;
}

void s6ps_wchan_finish (s6ps_wchan_t *w)
{
  genalloc_free(size_t, &w->ind) ;
  stralloc_free(&w->sysmap) ;
}

static inline size_t lookup (s6ps_wchan_t const *w, uint64_t addr, size_t *i)
{
  size_t low = 0, mid, high = genalloc_len(size_t, &w->ind), len ;
  for (;;)
  {
    uint64_t cur ;
    mid = (low + high) >> 1 ;
    len = uint64_xscan(w->sysmap.s + genalloc_s(size_t, &w->ind)[mid], &cur) ;
    if (!len) return 0 ;
    if (cur == addr) break ;
    if (mid == low) return 0 ;
    if (addr < cur) high = mid ; else low = mid ;
  }
  *i = mid ;
  return len ;
}

int s6ps_wchan_lookup (s6ps_wchan_t const *w, stralloc *sa, uint64_t addr)
{
  if (addr == (sizeof(void *) == 8 ? 0xffffffffffffffffULL : 0xffffffffUL))
    return stralloc_catb(sa, "*", 1) ;
  if (!addr) return stralloc_catb(sa, "-", 1) ;
  if (w->sysmap.len)
  {
    size_t i, pos, len = lookup(w, addr, &i) ;
    if (!len) return stralloc_catb(sa, "?", 1) ;
    pos = genalloc_s(size_t, &w->ind)[i] + len + 3 ;
    return stralloc_catb(sa, w->sysmap.s + pos, genalloc_s(size_t, &w->ind)[i+1] - 1 - pos) ;
  }
  if (!stralloc_readyplus(sa, UINT64_FMT + 3)) return 0 ;
  stralloc_catb(sa, "(0x", 3) ;
  sa->len += uint64_fmt(sa->s + sa->len, addr) ;
  stralloc_catb(sa, ")", 1) ;
  return 1 ;
}

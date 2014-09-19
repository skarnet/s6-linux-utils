/* ISC license. */

#include <sys/utsname.h>
#include <skalibs/uint64.h>
#include <skalibs/bytestr.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include "s6-ps.h"

static stralloc sysmap = STRALLOC_ZERO ;
static genalloc ind = GENALLOC_ZERO ;

int s6ps_wchan_init (char const *file)
{
  if (file)
  {
    if (!openslurpclose(&sysmap, file)) return 0 ;
  }
  else
  {
    char *files[3] = { "/proc/kallsyms", 0, "/boot/System.map" } ;
    struct utsname uts ;
    unsigned int n ;
    if (uname(&uts) < 0) return 0 ;
    n = str_len(uts.release) ;
    {
      char buf[18 + n] ;
      register unsigned int i = 0 ;
      byte_copy(buf, 16, "/boot/System.map") ;
      buf[16] = '-' ;
      byte_copy(buf + 17, n + 1, uts.release) ;
      files[1] = buf ;
      for (; i < 3 ; i++)
        if (openslurpclose(&sysmap, files[i])) break ;
      if (i >= 3) return 0 ;
    }
  }
  {
    unsigned int i = 0 ;
    if (!genalloc_append(unsigned int, &ind, &i)) goto err2 ;
    for (i = 1 ; i <= sysmap.len ; i++)
      if (sysmap.s[i-1] == '\n')
        if (!genalloc_append(unsigned int, &ind, &i)) goto err ;
  }
  return 1 ;
 err:
  genalloc_free(unsigned int, &ind) ;
 err2:
  stralloc_free(&sysmap) ;
  return 0 ;
}

void s6ps_wchan_finish (void)
{
  genalloc_free(unsigned int, &ind) ;
  stralloc_free(&sysmap) ;
}

static inline unsigned int lookup (uint64 addr, unsigned int *i)
{
  unsigned int low = 0, mid, high = genalloc_len(unsigned int, &ind), len ;
  for (;;)
  {
    uint64 cur ;
    mid = (low + high) >> 1 ;
    len = uint64_xscan(sysmap.s + genalloc_s(unsigned int, &ind)[mid], &cur) ;
    if (!len) return 0 ;
    if (cur == addr) break ;
    if (mid == low) return 0 ;
    if (addr < cur) high = mid ; else low = mid ;
  }
  *i = mid ;
  return len ;
}

int s6ps_wchan_lookup (stralloc *sa, uint64 addr)
{
  if (addr == (sizeof(void *) == 8 ? 0xffffffffffffffffULL : 0xffffffffUL))
    return stralloc_catb(sa, "*", 1) ;
  if (!addr) return stralloc_catb(sa, "-", 1) ;
  if (sysmap.len)
  {
    unsigned int i ;
    unsigned int len = lookup(addr, &i) ;
    register unsigned int pos ;
    if (!len) return stralloc_catb(sa, "?", 1) ;
    pos = genalloc_s(unsigned int, &ind)[i] + len + 3 ;
    return stralloc_catb(sa, sysmap.s + pos, genalloc_s(unsigned int, &ind)[i+1] - 1 - pos) ;
  }
  if (!stralloc_readyplus(sa, UINT64_FMT + 3)) return 0 ;
  stralloc_catb(sa, "(0x", 3) ;
  sa->len += uint64_fmt(sa->s + sa->len, addr) ;
  stralloc_catb(sa, ")", 1) ;
  return 1 ;
}

/* ISC license. */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/uint32.h>
#include <skalibs/diuint32.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>
#include <skalibs/avltree.h>
#include "s6-ps.h"

static avltree ttycache_tree = AVLTREE_ZERO ;
static genalloc ttycache_index = GENALLOC_ZERO ;

static void *left32_dtok (unsigned int d, void *x)
{
  return (void *)&genalloc_s(diuint32, (genalloc *)x)[d].left ;
}

static int uint32_cmp (void const *a, void const *b, void *x)
{
  register uint32 aa = *(uint32 *)a ;
  register uint32 bb = *(uint32 *)b ;
  (void)x ;
  return (aa < bb) ? -1 : (aa > bb) ;
}

int s6ps_ttycache_init (void)
{
  avltree_init(&ttycache_tree, 5, 3, 8, &left32_dtok, &uint32_cmp, &ttycache_index) ;
  return 1 ;
}

void s6ps_ttycache_finish (void)
{
  avltree_free(&ttycache_tree) ;
  genalloc_free(diuint, &ttycache_index) ;
}

static int check (char const *s, uint32 ttynr)
{
  struct stat st ;
  if (stat(s, &st) < 0) return 0 ;
  return S_ISCHR(st.st_mode) && (st.st_rdev == ttynr) ;
}


 /* No blind scanning of all /dev or /sys/devices, kthx */

static int ttyguess (stralloc *sa, uint32 ttynr)
{
  unsigned int maj = major(ttynr), min = minor(ttynr) ;

 /* Try /dev/tty? and /dev/pts/? */
  if (maj == 4 && min < 64)
  {
    char tmp[11] = "/dev/tty" ;
    tmp[uint_fmt(tmp+8, min)] = 0 ;
    if (check(tmp, ttynr)) return stralloc_cats(sa, tmp+5) && stralloc_0(sa) ;
  }
  else if (maj >= 136 && maj < 144)
  {
    char tmp[9 + UINT_FMT] = "/dev/pts/" ;
    register unsigned int n = ((maj - 136) << 20) | min ;
    tmp[9 + uint_fmt(tmp+9, n)] = 0 ;
    if (check(tmp, ttynr)) return stralloc_cats(sa, tmp+5) && stralloc_0(sa) ;
  }

 /* Use /sys/dev/char/maj:min if it exists */
  {
    int fd ;
    char path[23 + 2 * UINT_FMT] = "/sys/dev/char/" ;
    register unsigned int pos = 14 ;
    pos += uint_fmt(path + pos, maj) ;
    path[pos++] = ':' ;
    pos += uint_fmt(path + pos, min) ;
    byte_copy(path + pos, 8, "/uevent") ;
    fd = open_read(path) ;
    if (fd >= 0)
    {
      char buf[4097] ;
      buffer b = BUFFER_INIT(&buffer_read, fd, buf, 4097) ;
      unsigned int start = satmp.len ;
      register int r ;
      for (;;)
      {
        satmp.len = start ;
        r = skagetln(&b, &satmp, '\n') ;
        if (r <= 0) break ;
        if ((satmp.len - start) > 8 && !byte_diff(satmp.s + start, 8, "DEVNAME=")) break ;
      }
      fd_close(fd) ;
      if (r > 0)
      {
        satmp.s[satmp.len - 1] = 0 ;
        satmp.len = start ;
        byte_copy(satmp.s + start + 3, 5, "/dev/") ;
        if (check(satmp.s + start + 3, ttynr))
          return stralloc_cats(sa, satmp.s + start + 8) && stralloc_0(sa) ;
      }
    }
  }

 /* Fallback: print explicit maj:min */
  {
    char tmp[3 + 2 * UINT_FMT] = "(" ;
    register unsigned int pos = 1 ;
    pos += uint_fmt(tmp + pos, maj) ;
    tmp[pos++] = ':' ;
    pos += uint_fmt(tmp + pos, min) ;
    tmp[pos++] = ')' ;
    tmp[pos++] = 0 ;
    return stralloc_catb(sa, tmp, pos) ;
  }
}

int s6ps_ttycache_lookup (stralloc *sa, uint32 ttynr)
{
  int wasnull = !satmp.s ;
  diuint32 d = { .left = ttynr, .right = satmp.len } ;
  unsigned int i ;
  if (!avltree_search(&ttycache_tree, &d.left, &i))
  {
    unsigned int n = genalloc_len(diuint32, &ttycache_index) ;
    if (!ttyguess(&satmp, ttynr)) return 0 ;
    if (!genalloc_append(diuint32, &ttycache_index, &d)) goto err ;
    if (!avltree_insert(&ttycache_tree, n))
    {
      genalloc_setlen(diuint32, &ttycache_index, n) ;
      goto err ;
    }
    i = n ;
  }
  return stralloc_cats(sa, satmp.s + genalloc_s(diuint32, &ttycache_index)[i].right) ;
 err:
  {
    register int e = errno ;
    if (wasnull) stralloc_free(&satmp) ;
    else satmp.len = d.right ;
    errno = e ;
  }
  return 0 ;
}

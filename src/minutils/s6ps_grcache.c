/* ISC license. */

#include <sys/types.h>
#include <grp.h>
#include <errno.h>
#include <skalibs/uint.h>
#include <skalibs/diuint.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/skamisc.h>
#include <skalibs/avltree.h>
#include "s6-ps.h"

static avltree grcache_tree = AVLTREE_ZERO ;
static genalloc grcache_index = GENALLOC_ZERO ;

int s6ps_grcache_init (void)
{
  avltree_init(&grcache_tree, 5, 3, 8, &left_dtok, &uint_cmp, &grcache_index) ;
  return 1 ;
}

void s6ps_grcache_finish (void)
{
  avltree_free(&grcache_tree) ;
  genalloc_free(diuint, &grcache_index) ;
}

int s6ps_grcache_lookup (stralloc *sa, unsigned int gid)
{
  int wasnull = !satmp.s ;
  diuint d = { .left = gid, .right = satmp.len } ;
  unsigned int i ;
  if (!avltree_search(&grcache_tree, &d.left, &i))
  {
    struct group *gr ;
    unsigned int n = genalloc_len(diuint, &grcache_index) ;
    errno = 0 ;
    gr = getgrgid(gid) ;
    if (!gr)
    {
      if (errno) return 0 ;
      if (!stralloc_readyplus(&satmp, UINT_FMT + 2)) return 0 ;
      stralloc_catb(&satmp, "(", 1) ;
      satmp.len += uint_fmt(satmp.s + satmp.len, gid) ;
      stralloc_catb(&satmp, ")", 2) ;
    }
    else if (!stralloc_cats(&satmp, gr->gr_name) || !stralloc_0(&satmp)) return 0 ;
    if (!genalloc_append(diuint, &grcache_index, &d)) goto err ;
    if (!avltree_insert(&grcache_tree, n))
    {
      genalloc_setlen(diuint, &grcache_index, n) ;
      goto err ;
    }
    i = n ;
  }
  return stralloc_cats(sa, satmp.s + genalloc_s(diuint, &grcache_index)[i].right) ;
 err:
  {
    register int e = errno ;
    if (wasnull) stralloc_free(&satmp) ;
    else satmp.len = d.right ;
    errno = e ;
  }
  return 0 ;
}

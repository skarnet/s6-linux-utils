/* ISC license. */

#include <stdint.h>

#include <grp.h>
#include <errno.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/skamisc.h>
#include <skalibs/avltree.h>

#include "s6-ps.h"

static avltree grcache_tree = AVLTREE_ZERO ;
static genalloc grcache_index = GENALLOC_ZERO ;

int s6ps_grcache_init (void)
{
  avltree_init(&grcache_tree, 5, 3, 8, &left_dtok, &uint32_cmp, &grcache_index) ;
  return 1 ;
}

void s6ps_grcache_finish (void)
{
  avltree_free(&grcache_tree) ;
  genalloc_free(dius_t, &grcache_index) ;
}

int s6ps_grcache_lookup (stralloc *sa, gid_t gid)
{
  int wasnull = !satmp.s ;
  dius_t d = { .left = (uint32_t)gid, .right = satmp.len } ;
  uint32_t i ;
  if (!avltree_search(&grcache_tree, &d.left, &i))
  {
    struct group *gr ;
    unsigned int n = genalloc_len(dius_t, &grcache_index) ;
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
    if (!genalloc_append(dius_t, &grcache_index, &d)) goto err ;
    if (!avltree_insert(&grcache_tree, n))
    {
      genalloc_setlen(dius_t, &grcache_index, n) ;
      goto err ;
    }
    i = n ;
  }
  return stralloc_cats(sa, satmp.s + genalloc_s(dius_t, &grcache_index)[i].right) ;
 err:
  {
    int e = errno ;
    if (wasnull) stralloc_free(&satmp) ;
    else satmp.len = d.right ;
    errno = e ;
  }
  return 0 ;
}

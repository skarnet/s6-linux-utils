/* ISC license. */

#include <sys/types.h>
#include <stdint.h>
#include <pwd.h>
#include <errno.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/skamisc.h>
#include <skalibs/avltree.h>
#include "s6-ps.h"

static avltree pwcache_tree = AVLTREE_ZERO ;
static genalloc pwcache_index = GENALLOC_ZERO ;

int s6ps_pwcache_init (void)
{
  avltree_init(&pwcache_tree, 5, 3, 8, &left_dtok, &uint32_cmp, &pwcache_index) ;
  return 1 ;
}

void s6ps_pwcache_finish (void)
{
  avltree_free(&pwcache_tree) ;
  genalloc_free(dius_t, &pwcache_index) ;
}

int s6ps_pwcache_lookup (stralloc *sa, uid_t uid)
{
  int wasnull = !satmp.s ;
  dius_t d = { .left = (uint32_t)uid, .right = satmp.len } ;
  uint32_t i ;
  if (!avltree_search(&pwcache_tree, &d.left, &i))
  {
    struct passwd *pw ;
    unsigned int n = genalloc_len(dius_t, &pwcache_index) ;
    errno = 0 ;
    pw = getpwuid(uid) ;
    if (!pw)
    {
      if (errno) return 0 ;
      if (!stralloc_readyplus(&satmp, UINT_FMT + 2)) return 0 ;
      stralloc_catb(&satmp, "(", 1) ;
      satmp.len += uint_fmt(satmp.s + satmp.len, uid) ;
      stralloc_catb(&satmp, ")", 2) ;
    }
    else if (!stralloc_cats(&satmp, pw->pw_name) || !stralloc_0(&satmp)) return 0 ;
    if (!genalloc_append(dius_t, &pwcache_index, &d)) goto err ;
    if (!avltree_insert(&pwcache_tree, n))
    {
      genalloc_setlen(dius_t, &pwcache_index, n) ;
      goto err ;
    }
    i = n ;
  }
  return stralloc_cats(sa, satmp.s + genalloc_s(dius_t, &pwcache_index)[i].right) ;
 err:
  {
    int e = errno ;
    if (wasnull) stralloc_free(&satmp) ;
    else satmp.len = d.right ;
    errno = e ;
  }
  return 0 ;
}

/* ISC license. */

#include <stdint.h>
#include <pwd.h>
#include <errno.h>

#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/skamisc.h>
#include <skalibs/avltree.h>

#include "s6ps.h"
#include "s6ps-internal.h"

int s6ps_pwcache_lookup (s6ps_cache_t *cache, stralloc *sa, uid_t uid)
{
  dius_t d = { .left = (uint32_t)uid, .right = satmp.len } ;
  uint32_t i ;
  if (!avltree_search(&cache->tree, &d.left, &i))
  {
    struct passwd *pw ;
    size_t n = genalloc_len(dius_t, &cache->index) ;
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
    if (!genalloc_append(dius_t, &cache->index, &d)) goto err ;
    if (!avltree_insert(&cache->tree, n))
    {
      genalloc_setlen(dius_t, &cache->index, n) ;
      goto err ;
    }
    i = n ;
  }
  return stralloc_cats(sa, satmp.s + genalloc_s(dius_t, &cache->index)[i].right) ;
 err:
  satmp.len = d.right ;
  return 0 ;
}

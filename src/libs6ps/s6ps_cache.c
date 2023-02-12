/* ISC license. */

#include <stdint.h>

#include <skalibs/genalloc.h>
#include <skalibs/avltree.h>

#include "s6ps.h"
#include "s6ps-internal.h"

static void *left_dtok (unsigned int d, void *x)
{
  return (void *)&genalloc_s(dius_t, (genalloc *)x)[d].left ;
}

int s6ps_uint32_cmp (void const *a, void const *b, void *x)
{
  uint32_t aa = *(uint32_t *)a ;
  uint32_t bb = *(uint32_t *)b ;
  (void)x ;
  return (aa < bb) ? -1 : (aa > bb) ;
}

int s6ps_cache_init (s6ps_cache_t *cache)
{
  avltree_init(&cache->tree, 5, 3, 8, &left_dtok, &s6ps_uint32_cmp, &cache->index) ;
  return 1 ;
}

void s6ps_cache_finish (s6ps_cache_t *cache)
{
  avltree_free(&cache->tree) ;
  genalloc_free(dius_t, &cache->index) ;
}

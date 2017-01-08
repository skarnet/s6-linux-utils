/* ISC license. */

#include <errno.h>
#include <skalibs/avltreen.h>
#include "s6-ps.h"

/* XXX: need to change all the types if the libdatastruct API changes */

typedef struct ptreeiter_s ptreeiter_t, *ptreeiter_t_ref ;
struct ptreeiter_s
{
  unsigned int *childlist ;
  unsigned int const *childindex ;
  unsigned int const *ppindex ;
  unsigned int *cpos ;
} ;

typedef struct pstuff_s pstuff_t, *pstuff_t_ref ;
struct pstuff_s
{
  unsigned int *orderedlist ;
  pscan_t *p ;
  unsigned int const *childlist ;
  unsigned int const *childindex ;
  unsigned int const *nchild ;
} ;

static int fillchildlist (unsigned int i, unsigned int h, void *x)
{
  register ptreeiter_t *pt = x ;
  register unsigned int j = pt->ppindex[i] ;
  pt->childlist[pt->childindex[j] + pt->cpos[j]++] = i ;
  (void)h ;
  return 1 ;
}

static void fillo_tree_rec (pstuff_t *blah, unsigned int root, signed int h)
{
  static unsigned int j = 0 ;
  register unsigned int i = !blah->p[root].pid ;
  if (blah->p[root].pid == 1) h = -1 ;
  blah->p[root].height = (h > 0) ? h : 0 ;
  blah->orderedlist[j++] = root ;
  for (; i < blah->nchild[root] ; i++)
    fillo_tree_rec(blah, blah->childlist[blah->childindex[root] + i], h+1) ;
}

 /*
    Fills up orderedlist with the right indices to print a process tree.
    O(n log n) time, O(n) space, all in the stack.
 */

void s6ps_otree (pscan_t *p, unsigned int n, avltreen *pidtree, unsigned int *orderedlist)
{
  unsigned int childlist[n] ;
  unsigned int childindex[n] ;
  unsigned int nchild[n] ;
  register unsigned int i = 0 ;
  for (; i < n ; i++) nchild[i] = 0 ;

 /* Compute the ppid tree */
  for (i = 0 ; i < n ; i++)
  {
    unsigned int k ;
    if (!avltreen_search(pidtree, &p[i].ppid, &k)) k = n-1 ;
    orderedlist[i] = k ; /* using orderedlist as ppindex */
    nchild[k]++ ;
  }
  {
    unsigned int j = 0 ;
    for (i = 0 ; i < n ; i++)
    {
      childindex[i] = j ;
      j += nchild[i] ;
    }
  }

 /* Fill the childlist by increasing pids so it is sorted */
  {
    unsigned int cpos[n] ;
    ptreeiter_t blah = { .childlist = childlist, .childindex = childindex, .ppindex = orderedlist, .cpos = cpos } ;
    for (i = 0 ; i < n ; i++) cpos[i] = 0 ;
    avltreen_iter_nocancel(pidtree, avltreen_totalsize(pidtree), &fillchildlist, &blah) ;
  }

 /* If we have init, make it the last in the orphan list */
  if (n > 1 && p[childlist[childindex[n-1]+1]].pid == 1)
  {
    unsigned int pos1 = childlist[childindex[n-1] + 1] ;
    for (i = 2 ; i < nchild[n-1] ; i++)
      childlist[childindex[n-1]+i-1] = childlist[childindex[n-1]+i] ;
    childlist[childindex[n-1]+nchild[n-1]-1] = pos1 ;
  } 

 /* Finally, fill orderedlist by walking the childindex tree. */
  {
    pstuff_t blah = { .orderedlist = orderedlist, .p = p, .childlist = childlist, .childindex = childindex, .nchild = nchild } ;
    fillo_tree_rec(&blah, n-1, -1) ;
  }
}

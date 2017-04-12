/* ISC license. */

#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include "s6-ps.h"

 /*
    going to great lengths to avoid scanf(), but all this code
    is still smaller than scanf (no floating point parsing etc.)
 */

#define STATVARS 49

typedef size_t scanfunc_t (char const *, void *) ;
typedef scanfunc_t *scanfunc_t_ref ;

static size_t f64 (char const *s, void *u64)
{
  uint64_t *u = u64 ;
  return uint64_scan(s, u) ;
}

#define DEFUN(name, type) \
static size_t name (char const *s, void *p) \
{ \
  uint64_t u ; \
  size_t len = uint64_scan(s, &u) ; \
  *(type *)p = u ; \
  return len ; \
} \

DEFUN(fint, int)
DEFUN(fpid, pid_t)
DEFUN(fdev, dev_t)

static scanfunc_t_ref scanfuncs[STATVARS] =
{
  &fpid, /* ppid */
  &fpid, /* pgrp */
  &fpid, /* session */
  &fdev, /* tty_nr */
  &fpid, /* tpgid */
  &f64, /* flags */
  &f64, /* minflt */
  &f64, /* cminflt */
  &f64, /* majflt */
  &f64, /* cmajflt */
  &f64, /* utime */
  &f64, /* stime */
  &f64, /* cutime */
  &f64, /* cstime */
  &fint, /* priority */
  &fint, /* nice */
  &f64, /* num_threads */
  &f64, /* itrealvalue */
  &f64, /* starttime */
  &f64, /* vsize */
  &f64, /* rss */
  &f64, /* rsslim */
  &f64, /* startcode */
  &f64, /* endcode */
  &f64, /* startstack */
  &f64, /* kstkesp */
  &f64, /* kstkeip */
  &f64, /* signal */
  &f64, /* blocked */
  &f64, /* sigignore */
  &f64, /* sigcatch */
  &f64, /* wchan */
  &f64, /* nswap */
  &f64, /* cnswap */
  &fint, /* exit_signal */
  &f64, /* processor */
  &f64, /* rt_priority */
  &f64, /* policy */
  &f64, /* delayacct_blkio_ticks */
  &f64, /* guest_time */
  &f64, /* cguest_time */
  &f64, /* start_data */
  &f64, /* end_data */
  &f64, /* start_brk */
  &f64, /* arg_start */
  &f64, /* arg_end */
  &f64, /* env_start */
  &f64, /* env_end */
  &fint /* exit_code */
} ;

int s6ps_statparse (pscan_t *p)
{
  uint64_t dummy64 ;
  int dummyint ;
  size_t pos = 0 ;
  void *scanresults[STATVARS] =
  {
    &p->ppid,
    &p->pgrp,
    &p->session,
    &p->ttynr,
    &p->tpgid,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &p->utime,
    &p->stime,
    &p->cutime,
    &p->cstime,
    &p->prio,
    &p->nice,
    &p->threads,
    &dummy64,
    &p->start,
    &p->vsize,
    &p->rss,
    &p->rsslim,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &p->wchan,
    &dummy64,
    &dummy64,
    &dummy64,
    &p->cpuno,
    &p->rtprio,
    &p->policy,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummyint
  } ;
  unsigned int i = 0 ;

  if (!p->statlen) return 0 ;
  pos = uint64_scan(p->data.s, &dummy64) ;
  if (!pos) return 0 ;
  if (dummy64 != p->pid) return 0 ;
  if (pos + 5 + p->commlen > p->statlen) return 0 ;
  if (p->data.s[pos++] != ' ') return 0 ;
  if (p->data.s[pos++] != '(') return 0 ;
  pos += p->commlen ;
  if (p->data.s[pos++] != ')') return 0 ;
  if (p->data.s[pos++] != ' ') return 0 ;
  p->state = pos++ ;
  for (; i < STATVARS ; i++)
  {
    size_t w ;
    if (pos + 1 > p->statlen) return 0 ;
    if (p->data.s[pos++] != ' ') return 0 ;
    w = (*scanfuncs[i])(p->data.s + pos, scanresults[i]) ;
    if (!w) return 0 ;
    pos += w ;
  }
  return 1 ;
}

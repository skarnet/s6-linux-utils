/* ISC license. */

#include <errno.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/fmtscan.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include "s6-ps.h"

 /*
    going to great lengths to avoid scanf(), but all this code
    is still smaller than scanf (no floating point parsing etc.)
 */

#define STATVARS 41

typedef unsigned int scanfunc_t (char const *, void *) ;
typedef scanfunc_t *scanfunc_t_ref ;

static unsigned int f32 (char const *s, void *u32)
{
  uint32 *u = u32 ;
  return uint32_scan(s, u) ;
}

static unsigned int f64 (char const *s, void *u64)
{
  uint64 *u = u64 ;
  return uint64_scan(s, u) ;
}

static unsigned int fint (char const *s, void *i)
{
  int *d = i ;
  return int_scan(s, d) ;
}

static scanfunc_t_ref scanfuncs[STATVARS] =
{
  &f32, /* ppid */
  &f32, /* pgrp */
  &f32, /* session */
  &f32, /* tty_nr */
  &fint, /* tpgid */
  &f32, /* flags */
  &f32, /* minflt */
  &f32, /* cminflt */
  &f32, /* majflt */
  &f32, /* cmajflt */
  &f64, /* utime */
  &f64, /* stime */
  &f64, /* cutime */
  &f64, /* cstime */
  &fint, /* priority */
  &fint, /* nice */
  &f32, /* num_threads */
  &f32, /* itrealvalue */
  &f64, /* starttime */
  &f64, /* vsize */
  &f64, /* rss */
  &f64, /* rsslim */
  &f64, /* startcode */
  &f64, /* endcode */
  &f64, /* startstack */
  &f64, /* kstkesp */
  &f64, /* kstkeip */
  &f32, /* signal */
  &f32, /* blocked */
  &f32, /* sigignore */
  &f32, /* sigcatch */
  &f64, /* wchan */
  &f32, /* nswap */
  &f32, /* cnswap */
  &f32, /* exit_signal */
  &f32, /* processor */
  &f32, /* rt_priority */
  &f32, /* policy */
  &f64, /* delayacct_blkio_ticks */
  &f32, /* guest_time */
  &f32 /* cguest_time */
} ;

int s6ps_statparse (pscan_t *p)
{
  uint64 dummy64 ;
  uint32 dummy32 ;
  unsigned int pos = 0 ;
  void *scanresults[STATVARS] =
  {
    &p->ppid,
    &p->pgrp,
    &p->session,
    &p->ttynr,
    &p->tpgid,
    &dummy32,
    &dummy32,
    &dummy32,
    &dummy32,
    &dummy32,
    &p->utime,
    &p->stime,
    &p->cutime,
    &p->cstime,
    &p->prio,
    &p->nice,
    &p->threads,
    &dummy32,
    &p->start,
    &p->vsize,
    &p->rss,
    &p->rsslim,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy64,
    &dummy32,
    &dummy32,
    &dummy32,
    &dummy32,
    &p->wchan,
    &dummy32,
    &dummy32,
    &dummy32,
    &p->cpuno,
    &p->rtprio,
    &p->policy,
    &dummy64,
    &dummy32,
    &dummy32
  } ;
  register unsigned int i = 0 ;

  if (!p->statlen) return 0 ;
  pos = uint32_scan(p->data.s, &dummy32) ;
  if (!pos) return 0 ;
  if (dummy32 != p->pid) return 0 ;
  if (pos + 5 + p->commlen > p->statlen) return 0 ;
  if (p->data.s[pos++] != ' ') return 0 ;
  if (p->data.s[pos++] != '(') return 0 ;
  pos += p->commlen ;
  if (p->data.s[pos++] != ')') return 0 ;
  if (p->data.s[pos++] != ' ') return 0 ;
  p->state = pos++ ;
  for (; i < STATVARS ; i++)
  {
    unsigned int w ;
    if (pos + 1 > p->statlen) return 0 ;
    if (p->data.s[pos++] != ' ') return 0 ;
    w = (*scanfuncs[i])(p->data.s + pos, scanresults[i]) ;
    if (!w) return 0 ; pos += w ;
  }
  return 1 ;
}

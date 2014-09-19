/* ISC license. */

#ifndef _S6PS_H_
#define _S6PS_H_

#include <sys/types.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include <skalibs/avltreen.h>

 /* pfield: the output fields */

typedef enum pfield_e pfield_t, *pfield_t_ref ;
enum pfield_e
{
  PFIELD_PID,
  PFIELD_COMM,
  PFIELD_STATE,
  PFIELD_PPID,
  PFIELD_PGRP,
  PFIELD_SESSION,
  PFIELD_TTY,
  PFIELD_TPGID, 
  PFIELD_UTIME,
  PFIELD_STIME,
  PFIELD_CUTIME,
  PFIELD_CSTIME,
  PFIELD_PRIO,
  PFIELD_NICE,
  PFIELD_THREADS,
  PFIELD_START,
  PFIELD_VSIZE,
  PFIELD_RSS,
  PFIELD_RSSLIM,
  PFIELD_CPUNO,
  PFIELD_RTPRIO,
  PFIELD_RTPOLICY,
  PFIELD_USER,
  PFIELD_GROUP,
  PFIELD_PMEM,
  PFIELD_WCHAN,
  PFIELD_ARGS,
  PFIELD_ENV,
  PFIELD_PCPU,
  PFIELD_TTIME,
  PFIELD_CTTIME,
  PFIELD_TSTART,
  PFIELD_CPCPU,
  PFIELD_PHAIL
} ;

extern char const *const *s6ps_opttable ;
extern char const *const *s6ps_fieldheaders ;

 /* pscan: the main structure */

typedef struct pscan_s pscan_t, *pscan_t_ref ;
struct pscan_s
{
  stralloc data ;
  unsigned int pid ;
  signed int height ;
  unsigned int statlen ;
  unsigned int commlen ;
  unsigned int cmdlen ;
  unsigned int envlen ;
  uid_t uid ;
  gid_t gid ;
  uint32 ppid ;
  unsigned int state ;
  uint32 pgrp ;
  uint32 session ;
  uint32 ttynr ;
  int tpgid ;
  uint64 utime ;
  uint64 stime ;
  uint64 cutime ;
  uint64 cstime ;
  int prio ;
  int nice ;
  uint32 threads ;
  uint64 start ;
  uint64 vsize ;
  uint64 rss ;
  uint64 rsslim ;
  uint64 wchan ;
  uint32 cpuno ;
  uint32 rtprio ;
  uint32 policy ;
} ;

#define PSCAN_ZERO \
{ \
  .data = STRALLOC_ZERO, \
  .pid = 0, \
  .height = 0, \
  .statlen = 0, \
  .commlen = 0, \
  .cmdlen = 0, \
  .envlen = 0, \
  .uid = 0, \
  .gid = 0, \
  .ppid = 0, \
  .state = 0, \
  .pgrp = 0, \
  .session = 0, \
  .ttynr = 0, \
  .tpgid = -1, \
  .utime = 0, \
  .stime = 0, \
  .cutime = 0, \
  .cstime = 0, \
  .prio = 0, \
  .nice = 0, \
  .threads = 0, \
  .start = 0, \
  .vsize = 0, \
  .rss = 0, \
  .rsslim = 0, \
  .wchan = 0, \
  .cpuno = 0, \
  .rtprio = 0, \
  .policy = 0 \
}

extern int s6ps_statparse (pscan_t *) ;
extern void s6ps_otree (pscan_t *, unsigned int, avltreen *, unsigned int *) ;

extern int s6ps_compute_boottime (pscan_t *, unsigned int) ;

typedef int pfieldfmt_func_t (pscan_t *, unsigned int *, unsigned int *) ;
typedef pfieldfmt_func_t *pfieldfmt_func_t_ref ;

extern pfieldfmt_func_t_ref *s6ps_pfield_fmt ;

extern void *left_dtok (unsigned int, void *) ;
extern int uint_cmp (void const *, void const *, void *) ;
extern int s6ps_pwcache_init (void) ;
extern void s6ps_pwcache_finish (void) ;
extern int s6ps_pwcache_lookup (stralloc *, unsigned int) ;
extern int s6ps_grcache_init (void) ;
extern void s6ps_grcache_finish (void) ;
extern int s6ps_grcache_lookup (stralloc *, unsigned int) ;
extern int s6ps_ttycache_init (void) ;
extern void s6ps_ttycache_finish (void) ;
extern int s6ps_ttycache_lookup (stralloc *, uint32) ;
extern int s6ps_wchan_init (char const *) ;
extern void s6ps_wchan_finish (void) ;
extern int s6ps_wchan_lookup (stralloc *, uint64) ;

#endif

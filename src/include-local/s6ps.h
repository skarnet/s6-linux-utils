/* ISC license. */

#ifndef S6PS_H
#define S6PS_H

#include <sys/types.h>
#include <stdint.h>

#include <skalibs/uint64.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/tai.h>
#include <skalibs/avltree.h>
#include <skalibs/avltreen.h>

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
  PFIELD_MINFLT,
  PFIELD_CMINFLT,
  PFIELD_MAJFLT,
  PFIELD_CMAJFLT, 
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

typedef struct pscan_s pscan_t, *pscan_t_ref ;
struct pscan_s
{
  stralloc data ;
  pid_t pid ;
  signed int height ;
  size_t statlen ;
  size_t commlen ;
  size_t cmdlen ;
  size_t envlen ;
  uid_t uid ;
  gid_t gid ;
  pid_t ppid ;
  unsigned int state ;
  pid_t pgrp ;
  pid_t session ;
  dev_t ttynr ;
  pid_t tpgid ;
  uint64_t minflt ;
  uint64_t cminflt ;
  uint64_t majflt ;
  uint64_t cmajflt ;
  uint64_t utime ;
  uint64_t stime ;
  uint64_t cutime ;
  uint64_t cstime ;
  int prio ;
  int nice ;
  uint64_t threads ;
  uint64_t start ;
  uint64_t vsize ;
  uint64_t rss ;
  uint64_t rsslim ;
  uint64_t wchan ;
  uint64_t cpuno ;
  uint64_t rtprio ;
  uint64_t policy ;
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
  .minflt = 0, \
  .cminflt = 0, \
  .majflt = 0, \
  .cmajflt = 0, \
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

typedef struct s6ps_cache_s s6ps_cache_t, *s6ps_cache_t_ref ;
struct s6ps_cache_s
{
  avltree tree ;
  genalloc index ;
} ;
#define S6PS_CACHE_ZERO { .tree = AVLTREE_ZERO, .index = GENALLOC_ZERO }

typedef struct s6ps_wchan_s s6ps_wchan_t, *s6ps_wchan_t_ref ;
struct s6ps_wchan_s
{
  stralloc sysmap ;
  genalloc ind ;
} ;
#define S6PS_WCHAN_ZERO { .sysmap = STRALLOC_ZERO, .ind = GENALLOC_ZERO }

typedef struct s6ps_auxinfo_s s6ps_auxinfo_t, *s6ps_auxinfo_t_ref ;
struct s6ps_auxinfo_s
{
  tain boottime ;
  s6ps_cache_t caches[3] ;
  s6ps_wchan_t wchan ;
  unsigned int hz ;
  unsigned int pgsz ;
  uint64_t totalmem ;
} ;
#define S6PS_AUXINFO_ZERO { .boottime = TAIN_EPOCH, .caches = { S6PS_CACHE_ZERO, S6PS_CACHE_ZERO, S6PS_CACHE_ZERO }, .wchan = S6PS_WCHAN_ZERO, .hz = 0, .pgsz = 0, .totalmem = 0 }

typedef int pfieldfmt_func (s6ps_auxinfo_t *, pscan_t *, size_t *, size_t *) ;
typedef pfieldfmt_func *pfieldfmt_func_ref ;


 /* exported by s6ps_pfield.c */

extern char const *const *s6ps_opttable ;
extern char const *const *s6ps_fieldheaders ;
extern pfieldfmt_func_ref const *const s6ps_pfield_fmt ;
extern int s6ps_compute_boottime (s6ps_auxinfo_t *, pscan_t *, unsigned int) ;


 /* exported by s6ps_statparse.c */

extern int s6ps_statparse (pscan_t *) ;


 /* exported by s6ps_otree.c */

extern void s6ps_otree (pscan_t *, unsigned int, avltreen *, unsigned int *) ;


 /* exported by s6ps_cache.c */

extern int s6ps_cache_init (s6ps_cache_t *) ;
extern void s6ps_cache_finish (s6ps_cache_t *) ;
extern int s6ps_uint32_cmp (void const *, void const *, void *) ;


 /* exported by s6ps_pwcache.c */

extern int s6ps_pwcache_lookup (s6ps_cache_t *, stralloc *, uid_t) ;


 /* exported by s6ps_grcache.c */

extern int s6ps_grcache_lookup (s6ps_cache_t *, stralloc *, gid_t) ;


 /* exported by s6ps_ttycache.c */

extern int s6ps_ttycache_lookup (s6ps_cache_t *, stralloc *, dev_t) ;


 /* exported by s6ps_wchan.c */

extern int s6ps_wchan_init (s6ps_wchan_t *, char const *) ;
extern void s6ps_wchan_finish (s6ps_wchan_t *) ;
extern int s6ps_wchan_lookup (s6ps_wchan_t const *, stralloc *, uint64_t) ;

#endif

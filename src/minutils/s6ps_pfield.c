/* ISC license. */

#include <unistd.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <skalibs/uint32.h>
#include <skalibs/uint64.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/ulong.h>
#include <skalibs/fmtscan.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/stralloc.h>
#include "s6-ps.h"

static char const *const fieldheaders[PFIELD_PHAIL] =
{
  "PID",
  "COMM",
  "STAT",
  "PPID",
  "PGRP",
  "SESSION",
  "TTY",
  "TPGID",
  "UTIME",
  "STIME",
  "CUTIME",
  "CSTIME",
  "PRIO",
  "NICE",
  "THREADS",
  "START",
  "VSZ",
  "RSS",
  "RSSLIM",
  "CPU",
  "RTPRIO",
  "RTPOLICY",
  "USER",
  "GROUP",
  "%MEM",
  "WCHAN",
  "COMMAND",
  "ENVIRONMENT",
  "%CPU",
  "TTIME",
  "CTTIME",
  "TSTART",
  "C%CPU"
} ;

char const *const *s6ps_fieldheaders = fieldheaders ;

static char const *const opttable[PFIELD_PHAIL] =
{
  "pid",
  "comm",
  "s",
  "ppid",
  "pgrp",
  "sess",
  "tty",
  "tpgid",
  "utime",
  "stime",
  "cutime",
  "cstime",
  "prio",
  "nice",
  "thcount",
  "start",
  "vsize",
  "rss",
  "rsslimit",
  "psr",
  "rtprio",
  "policy",
  "user",
  "group",
  "pmem",
  "wchan",
  "args",
  "env",
  "pcpu",
  "ttime",
  "cttime",
  "tstart",
  "cpcpu"
} ;

char const *const *s6ps_opttable = opttable ;

static tain_t boottime = TAIN_EPOCH ;

static int fmt_32 (pscan_t *p, unsigned int *pos, unsigned int *len, uint32 u)
{
  if (!stralloc_readyplus(&p->data, UINT32_FMT)) return 0 ;
  *pos = p->data.len ;
  *len = uint32_fmt(p->data.s + *pos, u) ;
  p->data.len += *len ;
  return 1 ;
}

static int fmt_64 (pscan_t *p, unsigned int *pos, unsigned int *len, uint64 u)
{                                                          
  if (!stralloc_readyplus(&p->data, UINT64_FMT)) return 0 ;
  *pos = p->data.len ;
  *len = uint64_fmt(p->data.s + *pos, u) ;
  p->data.len += *len ;
  return 1 ;
}                                                          

static int fmt_i (pscan_t *p, unsigned int *pos, unsigned int *len, int d)
{
  if (!stralloc_readyplus(&p->data, UINT32_FMT+1)) return 0 ;
  *pos = p->data.len ;
  *len = int_fmt(p->data.s + *pos, d) ;
  p->data.len += *len ;
  return 1 ;
}

static int fmt_pid (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->pid) ;
}

static int fmt_comm (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  *pos = p->statlen ;
  *len = p->commlen ;
  return 1 ;
}

static int fmt_s (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  if (!stralloc_readyplus(&p->data, 4)) return 0 ;
  *pos = p->data.len ;
  p->data.s[p->data.len++] = p->data.s[p->state] ;
  if (p->pid == p->session) p->data.s[p->data.len++] = 's' ;
  if (p->threads > 1) p->data.s[p->data.len++] = 'l' ;
  if ((p->tpgid > 0) && ((unsigned int)p->tpgid == p->pgrp))
    p->data.s[p->data.len++] = '+' ;
  if (p->nice) p->data.s[p->data.len++] = (p->nice < 0) ? '<' : 'N' ;
  
  *len = p->data.len - *pos ;
  return 1 ;
}

static int fmt_ppid (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->ppid) ;
}

static int fmt_pgrp (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->pgrp) ;
}

static int fmt_session (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->session) ;
}

static int fmt_ttynr(pscan_t *p, unsigned int *pos, unsigned int *len)
{
  if (p->ttynr)
  {
    unsigned int tmppos = p->data.len ;
    if (!s6ps_ttycache_lookup(&p->data, p->ttynr)) return 0 ;
    *pos = tmppos ;
    *len = p->data.len - tmppos ;
  }
  else
  {
    if (!stralloc_catb(&p->data, "-", 1)) return 0 ;
    *pos = p->data.len - 1 ;
    *len = 1 ;
  }
  return 1 ;
}

static int fmt_tpgid (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_i(p, pos, len, p->tpgid) ;
}

static unsigned int gethz (void)
{
  static unsigned int hz = 0 ;
  if (!hz)
  {            
    long jiffies = sysconf(_SC_CLK_TCK) ;
    if (jiffies < 1)
    {             
      char fmt[ULONG_FMT + 1] ;
      fmt[long_fmt(fmt, jiffies)] = 0 ;
      strerr_warnw3x("invalid _SC_CLK_TCK value (", fmt, "), using 100") ;
      hz = 100 ;
    }         
    else hz = (unsigned int)jiffies ;
  }
  return hz ;
}                

int s6ps_compute_boottime (pscan_t *p, unsigned int mypos)
{
  if (!mypos--)
  {
    strerr_warnwu1x("compute boot time - using epoch") ;
    return 0 ;
  }
  else
  {
    unsigned int hz = gethz() ;
    tain_t offset = { .sec = { .x = p[mypos].start / hz }, .nano = (p[mypos].start % hz) * (1000000000 / hz) } ;
    tain_sub(&boottime, &STAMP, &offset) ;
    return 1 ;
  }
}

static int fmt_jiffies (pscan_t *p, unsigned int *pos, unsigned int *len, uint64 j)
{
  unsigned int hz = gethz() ;
  uint32 hrs, mins, secs, hfrac ;
  if (!stralloc_readyplus(&p->data, UINT64_FMT + 13)) return 0 ;
  hfrac = (j % hz) * 100 / hz ;
  *pos = p->data.len ;
  j /= hz ;
  secs = j % 60 ; j /= 60 ;
  mins = j % 60 ; j /= 60 ;
  hrs = j % 24 ; j /= 24 ;
  if (j)
  {
    p->data.len += uint64_fmt(p->data.s + p->data.len, j) ;
    p->data.s[p->data.len++] = 'd' ;
  }
  if (j || hrs)
  {
    uint320_fmt(p->data.s + p->data.len, hrs, 2) ;
    p->data.len += 2 ;
    p->data.s[p->data.len++] = 'h' ;
  }
  if (j || hrs || mins)
  {
    uint320_fmt(p->data.s + p->data.len, mins, 2) ;
    p->data.len += 2 ;
    p->data.s[p->data.len++] = 'm' ;
  }
  uint320_fmt(p->data.s + p->data.len, secs, 2) ;
  p->data.len += 2 ;
  if (!j && !hrs && !mins)
  {
    p->data.s[p->data.len++] = '.' ;
    uint320_fmt(p->data.s + p->data.len, hfrac, 2) ;
    p->data.len += 2 ;
  }
  p->data.s[p->data.len++] = 's' ;
  *len = p->data.len - *pos ;
  return 1 ;
}

static int fmt_utime (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_jiffies(p, pos, len, p->utime) ;
}

static int fmt_stime (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_jiffies(p, pos, len, p->stime) ;
}

static int fmt_cutime (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_jiffies(p, pos, len, p->utime + p->cutime) ;
}

static int fmt_cstime (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_jiffies(p, pos, len, p->stime + p->cstime) ;
}

static int fmt_prio (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_i(p, pos, len, p->prio) ;
}

static int fmt_nice (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_i(p, pos, len, p->nice) ;
}

static int fmt_threads (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->threads) ;
}

static int fmt_timedate (pscan_t *p, unsigned int *pos, unsigned int *len, struct tm const *tm)
{
  static struct tm nowtm = { .tm_year = 0 } ;
  unsigned int tmplen ;
  char *tmpstrf = "%F" ;
  if (!nowtm.tm_year && !localtm_from_tai(&nowtm, tain_secp(&STAMP), 1)) return 0 ;
  if (!stralloc_readyplus(&p->data, 20)) return 0 ;
  if (tm->tm_year == nowtm.tm_year && tm->tm_yday == nowtm.tm_yday)
    tmpstrf = "%T" ;
  else if (tm->tm_year == nowtm.tm_year || (tm->tm_year+1 == nowtm.tm_year && (nowtm.tm_mon + 12 - tm->tm_mon) % 12 < 9))
    tmpstrf = "%b%d %R" ;
  tmplen = strftime(p->data.s + p->data.len, 20, tmpstrf, tm) ;
  if (!tmplen) return 0 ;
  *len = tmplen ;
  *pos = p->data.len ;
  p->data.len += tmplen ;
  return 1 ;
}

static int fmt_start (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  struct tm starttm ;
  unsigned int hz = gethz() ;
  tain_t blah = { .sec = { .x = p->start / hz }, .nano = (p->start % hz) * (1000000000 / hz) } ;
  tain_add(&blah, &boottime, &blah) ;
  if (!localtm_from_tai(&starttm, tain_secp(&blah), 1)) return 0 ;
  return fmt_timedate(p, pos, len, &starttm) ;
}

static unsigned int getpgsz (void)
{
  static unsigned int pgsz = 0 ;
  if (!pgsz)
  {
    long sz = sysconf(_SC_PAGESIZE) ;
    if (sz < 1)
    {
      char fmt[ULONG_FMT + 1] ;
      fmt[long_fmt(fmt, sz)] = 0 ;
      strerr_warnw3x("invalid _SC_PAGESIZE value (", fmt, "), using 4096") ;
      pgsz = 4096 ;
    }
    else pgsz = sz ;
  }
  return pgsz ;
}

static int fmt_vsize (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_64(p, pos, len, p->vsize / 1024) ;
}

static int fmt_rss (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_64(p, pos, len, p->rss * (getpgsz() / 1024)) ;
}

static int fmt_rsslim (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_64(p, pos, len, p->rsslim / 1024) ;
}

static int fmt_cpuno (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->cpuno) ;
}

static int fmt_rtprio (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_32(p, pos, len, p->rtprio) ;
}

static int fmt_policy (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  static char const *const policies[8] = { "NORMAL", "FIFO", "RR", "BATCH", "ISO", "IDLE", "UNKNOWN", "UNKNOWN" } ;
  unsigned int tmppos = p->data.len ;
  if (!stralloc_cats(&p->data, policies[p->policy & 7])) return 0 ;
  *pos = tmppos ;
  *len = p->data.len - tmppos ;
  return 1 ;
}

static int fmt_user (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  unsigned int tmppos = p->data.len ;
  if (!s6ps_pwcache_lookup(&p->data, p->uid)) return 0 ;
  *pos = tmppos ;
  *len = p->data.len - tmppos ;
  return 1 ;
}

static int fmt_group (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  unsigned int tmppos = p->data.len ;
  if (!s6ps_grcache_lookup(&p->data, p->gid)) return 0 ;
  *pos = tmppos ;
  *len = p->data.len - tmppos ;
  return 1 ;
}

static struct sysinfo si = { .totalram = 0, .loads = { 0, 0, 0 } } ;

static uint64 gettotalmem (void)
{
  uint64 totalmem = 0 ;
  if (!si.totalram && (sysinfo(&si) < 0)) return 0 ;
  totalmem = si.totalram ;
  totalmem *= si.mem_unit ;
  return totalmem ;
}

static int percent (stralloc *sa, unsigned int n, unsigned int *pos, unsigned int *len)
{
  if (!stralloc_readyplus(sa, UINT64_FMT+1)) return 0 ;
  *pos = sa->len ;
  sa->len += uint64_fmt(sa->s + sa->len, n / 100) ;
  sa->s[sa->len++] = '.' ;
  uint320_fmt(sa->s + sa->len, (uint32)(n % 100), 2) ;
  sa->len += 2 ;
  *len = sa->len - *pos ;
  return 1 ;
}

static int fmt_pmem (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  uint64 l = gettotalmem() ;
  return l ? percent(&p->data, p->rss * getpgsz() * 10000 / l, pos, len) : 0 ;
}

static int fmt_wchan (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  unsigned int tmppos = p->data.len ;
  if (!s6ps_wchan_lookup(&p->data, p->wchan)) return 0 ;
  *len = p->data.len - tmppos ;
  *pos = tmppos ;
  return 1 ;
}

static int fmt_args (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  if (!stralloc_readyplus(&p->data, (p->height << 2) + (p->cmdlen ? p->cmdlen : p->commlen + (p->data.s[p->state] == 'Z' ? 11 : 3))))
    return 0 ;
  *pos = p->data.len ;
  if (p->height)
  {
    register unsigned int i = 0 ;
    for (; i < 4 * (unsigned int)p->height - 3 ; i++)
      p->data.s[p->data.len + i] = ' ' ;
    byte_copy(p->data.s + p->data.len + 4 * p->height - 3, 3, "\\_ ") ;
    p->data.len += p->height << 2 ;
  }
  if (p->cmdlen)
  {
    register char const *r = p->data.s + p->statlen + p->commlen ;
    register char *w = p->data.s + p->data.len ;
    register unsigned int i = p->cmdlen ;
    while (i--)
    {
      register char c = *r++ ;
      *w++ = c ? c : ' ' ;
    }
    p->data.len += p->cmdlen ;
  }
  else if (p->data.s[p->state] == 'Z')
  {
    stralloc_catb(&p->data, p->data.s + uint32_fmt(0, p->pid) + 2, p->commlen) ;
    stralloc_catb(&p->data, " <defunct>", 10) ;
  }
  else
    stralloc_catb(&p->data, p->data.s + uint32_fmt(0, p->pid) + 1, p->commlen+2) ;
  *len = p->data.len - *pos ;
  return 1 ;
}

static int fmt_env (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  register unsigned int i = 0 ;
  if (!p->envlen)
  {
    if (!stralloc_catb(&p->data, "*", 1)) return 0 ;
    *pos = p->data.len - 1 ;
    *len = 1 ;
    return 1 ;
  }
  *pos = p->statlen + p->commlen + p->cmdlen ;
  *len = p->envlen ;
  for (; i < *len ; i++)
    if (!p->data.s[*pos + i]) p->data.s[*pos + i] = ' ' ;
  return 1 ;
}

static uint64 gettotalj (uint64 j)
{
  tain_t totaltime ;
  register unsigned int hz = gethz() ;
  tain_sub(&totaltime, &STAMP, &boottime) ;
  j = totaltime.sec.x * hz + totaltime.nano / (1000000000 / hz) - j ;
  if (!j) j = 1 ;
  return j ;
}

static int fmt_pcpu (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return percent(&p->data, 10000 * (p->utime + p->stime) / gettotalj(p->start), pos, len) ;
}


static int fmt_ttime (pscan_t *p, unsigned int *pos, unsigned int *len) 
{
  return fmt_jiffies(p, pos, len, p->utime + p->stime) ;
}

static int fmt_cttime (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return fmt_jiffies(p, pos, len, p->utime + p->stime + p->cutime + p->cstime) ;
}

static int fmt_tstart (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  unsigned int hz = gethz() ;
  tain_t blah = { .sec = { .x = p->start / hz }, .nano = (p->start % hz) * (1000000000 / hz) } ;
  if (!stralloc_readyplus(&p->data, TIMESTAMP)) return 0 ;
  tain_add(&blah, &boottime, &blah) ;
  *pos = p->data.len ;
  *len = timestamp_fmt(p->data.s + p->data.len, &blah) ;
  p->data.len += *len ;
  return 1 ;
}

static int fmt_cpcpu (pscan_t *p, unsigned int *pos, unsigned int *len)
{
  return percent(&p->data, 10000 * (p->utime + p->stime + p->cutime + p->cstime) / gettotalj(p->start), pos, len) ;
}

static pfieldfmt_func_t_ref pfieldfmt_table[PFIELD_PHAIL] =
{
  &fmt_pid,
  &fmt_comm,
  &fmt_s,
  &fmt_ppid,
  &fmt_pgrp,
  &fmt_session,
  &fmt_ttynr,
  &fmt_tpgid,
  &fmt_utime,
  &fmt_stime,
  &fmt_cutime,
  &fmt_cstime,
  &fmt_prio,
  &fmt_nice,
  &fmt_threads,
  &fmt_start,
  &fmt_vsize,
  &fmt_rss,
  &fmt_rsslim,
  &fmt_cpuno,
  &fmt_rtprio,
  &fmt_policy,
  &fmt_user,
  &fmt_group,
  &fmt_pmem,
  &fmt_wchan,
  &fmt_args,
  &fmt_env,
  &fmt_pcpu,
  &fmt_ttime,
  &fmt_cttime,
  &fmt_tstart,
  &fmt_cpcpu
} ;

pfieldfmt_func_t_ref *s6ps_pfield_fmt = pfieldfmt_table ;

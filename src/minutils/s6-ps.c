/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <dirent.h>

#include <skalibs/uint64.h>
#include <skalibs/types.h>
#include <skalibs/fmtscan.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/diuint.h>
#include <skalibs/tai.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/direntry.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>
#include <skalibs/unix-transactional.h>
#include <skalibs/avltreen.h>
#include "s6-ps.h"

#define USAGE "s6-ps [ -H ] [ -w spacing ] [ -W wchanfile ] [ -l | -o field,field... ]"

#define RIGHTFORMATTED ( \
  (1 << PFIELD_PID) | \
  (1 << PFIELD_PPID) | \
  (1 << PFIELD_PGRP) | \
  (1 << PFIELD_SESSION) | \
  (1 << PFIELD_TPGID) | \
  (1 << PFIELD_PRIO) | \
  (1 << PFIELD_NICE) | \
  (1 << PFIELD_THREADS) | \
  (1 << PFIELD_VSIZE) | \
  (1 << PFIELD_RSS) | \
  (1 << PFIELD_RSSLIM) | \
  (1 << PFIELD_CPUNO) | \
  (1 << PFIELD_RTPRIO) | \
  (1 << PFIELD_PMEM) | \
  (1 << PFIELD_PCPU) | \
  ((uint64)1 << PFIELD_CPCPU))

void *left_dtok (unsigned int d, void *x)
{
  return (void *)&genalloc_s(dius_t, (genalloc *)x)[d].left ;
}

int uint32_cmp (void const *a, void const *b, void *x)
{
  uint32_t aa = *(uint32_t *)a ;
  uint32_t bb = *(uint32_t *)b ;
  (void)x ;
  return (aa < bb) ? -1 : (aa > bb) ;
}

static void *pid_dtok (unsigned int d, void *x)
{
  return &((pscan_t *)x)[d].pid ;
}

static int fillo_notree (unsigned int i, unsigned int h, void *x)
{
  static unsigned int j = 0 ;
  unsigned int *list = x ;
  list[j++] = i ;
  (void)h ;
  return 1 ;
}

static inline unsigned int fieldscan (char const *s, pfield_t *list, uint64_t *fbf)
{
  uint64_t bits = 0 ;
  unsigned int n = 0 ;
  int cont = 1 ;
  for (; cont ; n++)
  {
    size_t len = str_chr(s, ',') ;
    pfield_t i = 0 ;
    if (!len) strerr_dief3x(100, "invalid", " (empty)", " field for -o option") ;
    if (!s[len]) cont = 0 ;
    {
      char tmp[len+1] ;
      memcpy(tmp, s, len) ;
      tmp[len] = 0 ;
      for (; i < PFIELD_PHAIL ; i++) if (!strcmp(tmp, s6ps_opttable[i])) break ;
      if (i >= PFIELD_PHAIL)
        strerr_dief4x(100, "invalid", " field for -o option", ": ", tmp) ;
      if (bits & (1 << i))
        strerr_dief4x(100, "duplicate", " field for -o option", ": ", tmp) ;
    }
    s += len + 1 ;
    list[n] = i ;
    bits |= (1 << i) ;
  }
  *fbf = bits ;
  return n ;
}

static int slurpit (unsigned int dirfd, stralloc *data, char const *buf, char const *what, size_t *len)
{
  size_t start = data->len ;
  int fd = open_readat(dirfd, what) ;
  if (fd < 0) return 0 ;
  if (!slurp(data, fd)) strerr_diefu4sys(111, "slurp ", buf, "/", what) ;
  fd_close(fd) ;
  *len = data->len - start ;
  return 1 ;
}

int main (int argc, char const *const *argv)
{
  genalloc pscans = GENALLOC_ZERO ; /* array of pscan_t */
  pfield_t fieldlist[PFIELD_PHAIL] = { PFIELD_USER, PFIELD_PID, PFIELD_TTY, PFIELD_STATE, PFIELD_START, PFIELD_ARGS } ;
  uint64_t fbf = (1 << PFIELD_USER) | (1 << PFIELD_PID) | (1 << PFIELD_TTY) | (1 << PFIELD_STATE) | (1 << PFIELD_START) | (1 << PFIELD_ARGS) ;
  size_t mypos = 0 ;
  unsigned int nfields = 6 ;
  pscan_t *p ;
  size_t n ;
  unsigned int spacing = 2 ;
  int flagtree = 0 ;
  char const *wchanfile = 0 ;
  int needstat ;
  PROG = "s6-ps" ;

  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "Hlw:W:o:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'H' : flagtree = 1 ; break ;
        case 'l' :
        {
          nfields = 11 ;
          fbf = (1 << PFIELD_USER) | (1 << PFIELD_PID) | ((uint64)1 << PFIELD_CPCPU) | (1 << PFIELD_PMEM) | (1 << PFIELD_VSIZE) | (1 << PFIELD_RSS) | (1 << PFIELD_TTY) | (1 << PFIELD_STATE) | (1 << PFIELD_START) | (1 << PFIELD_CTTIME) | (1 << PFIELD_ARGS) ;
          fieldlist[0] = PFIELD_USER ;
          fieldlist[1] = PFIELD_PID ;
          fieldlist[2] = PFIELD_CPCPU ;
          fieldlist[3] = PFIELD_PMEM ;
          fieldlist[4] = PFIELD_VSIZE ;
          fieldlist[5] = PFIELD_RSS ;
          fieldlist[6] = PFIELD_TTY ;
          fieldlist[7] = PFIELD_STATE ;
          fieldlist[8] = PFIELD_START ;
          fieldlist[9] = PFIELD_CTTIME ;
          fieldlist[10] = PFIELD_ARGS ;
          break ;
        }
        case 'w' :
        {
          if (!uint0_scan(l.arg, &spacing)) strerr_dieusage(100, USAGE) ;
          break ;
        }
        case 'W' : wchanfile = l.arg ; break ;
        case 'o' :
        {
          nfields = fieldscan(l.arg, fieldlist, &fbf) ;
          break ;
        }
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (!spacing) spacing = 1 ;
  if (spacing > 256) spacing = 256 ;

  needstat = flagtree || !!(fbf & (
   (1 << PFIELD_PID) |
   (1 << PFIELD_COMM) |
   (1 << PFIELD_STATE) |
   (1 << PFIELD_PPID) |
   (1 << PFIELD_PGRP) |
   (1 << PFIELD_SESSION) |
   (1 << PFIELD_TTY) |
   (1 << PFIELD_TPGID) |
   (1 << PFIELD_UTIME) |
   (1 << PFIELD_STIME) |
   (1 << PFIELD_CUTIME) |
   (1 << PFIELD_CSTIME) |
   (1 << PFIELD_PRIO) |
   (1 << PFIELD_NICE) |
   (1 << PFIELD_THREADS) |
   (1 << PFIELD_START) |
   (1 << PFIELD_VSIZE) |
   (1 << PFIELD_RSS) |
   (1 << PFIELD_RSSLIM) |
   (1 << PFIELD_CPUNO) |
   (1 << PFIELD_RTPRIO) |
   (1 << PFIELD_RTPOLICY) |
   (1 << PFIELD_PMEM) |
   (1 << PFIELD_WCHAN) |
   (1 << PFIELD_PCPU) |
   (1 << PFIELD_TTIME) |
   (1 << PFIELD_CTTIME) |
   ((uint64)1 << PFIELD_TSTART) |
   ((uint64)1 << PFIELD_CPCPU))) ;


 /* Scan /proc */

  {
    int needstatdir = !!(fbf & ((1 << PFIELD_USER) | (1 << PFIELD_GROUP))) ;
    pid_t mypid = getpid() ;
    DIR *dir = opendir("/proc") ;
    direntry *d ;
    char buf[25] = "/proc/" ;

    if (!dir) strerr_diefu1sys(111, "open /proc") ;
    for (;;)
    {
      pscan_t pscan = PSCAN_ZERO ;
      uint64_t u ;
      int dirfd ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (!uint640_scan(d->d_name, &u)) continue ;
      pscan.pid = u ;
      strcpy(buf+6, d->d_name) ;
      dirfd = open_read(buf) ;
      if (dirfd < 0) continue ;

      if (needstatdir)
      {
        struct stat st ;
        if (fstat(dirfd, &st) < 0) goto errindir ;
        pscan.uid = st.st_uid ;
        pscan.gid = st.st_gid ;
      }
      if (needstat)
      {
        if (!slurpit(dirfd, &pscan.data, buf, "stat", &pscan.statlen))
          goto errindir ;
        if (!slurpit(dirfd, &pscan.data, buf, "comm", &pscan.commlen))
          goto errindir ;
        if (pscan.commlen) { pscan.commlen-- ; pscan.data.len-- ; }
      }
      if (fbf & (1 << PFIELD_ARGS))
      {
        if (!slurpit(dirfd, &pscan.data, buf, "cmdline", &pscan.cmdlen)) goto errindir ;
        while (!pscan.data.s[pscan.data.len-1])
        {
          pscan.cmdlen-- ;
          pscan.data.len-- ;
        }
      }
      if (fbf & (1 << PFIELD_ENV)) slurpit(dirfd, &pscan.data, buf, "environ", &pscan.envlen) ;
      fd_close(dirfd) ;
      if (!genalloc_append(pscan_t, &pscans, &pscan))
        strerr_diefu1sys(111, "genalloc_append") ;
      if (pscan.pid == mypid) mypos = genalloc_len(pscan_t, &pscans) ;
      continue ;
     errindir:
      fd_close(dirfd) ;
      stralloc_free(&pscan.data) ;
    }
    if (errno) strerr_diefu1sys(111, "readdir /proc") ;
    dir_close(dir) ;
  }

 /* Add a process 0 as a root and sentinel */
  {
    pscan_t pscan = { .pid = 0, .ppid = 0 } ;
    if (!genalloc_append(pscan_t, &pscans, &pscan)) strerr_diefu1sys(111, "genalloc_append") ;
  }

  p = genalloc_s(pscan_t, &pscans) ;
  n = genalloc_len(pscan_t, &pscans) - 1 ;

  {
    unsigned int orderedlist[n+1] ; /* 1st element will be 0, ignored */
    unsigned int i = 0 ;

    /* Order the processes for display */

    {
      AVLTREEN_DECLARE_AND_INIT(pidtree, n+1, &pid_dtok, &uint32_cmp, p) ;
      for (i = 0 ; i < n ; i++)
      {
        if (needstat && !s6ps_statparse(p+i))
          strerr_diefu1sys(111, "parse process stats") ;
        if (!avltreen_insert(&pidtree, i))
          strerr_diefu1sys(111, "avltreen_insert") ;
      }
      if (!avltreen_insert(&pidtree, n))
        strerr_diefu1sys(111, "avltreen_insert") ;

      if (flagtree) s6ps_otree(p, n+1, &pidtree, orderedlist) ;
      else avltreen_iter_nocancel(&pidtree, avltreen_totalsize(&pidtree), &fillo_notree, orderedlist) ;
    }


   /* Format, compute length, output */

    if (fbf & ((1 << PFIELD_START) | ((uint64)1 << PFIELD_TSTART) | (1 << PFIELD_PCPU) | ((uint64)1 << PFIELD_CPCPU)))
    {
      tain_wallclock_read_g() ;
      s6ps_compute_boottime(p, mypos) ;
    }
    if (fbf & (1 << PFIELD_USER) && !s6ps_pwcache_init())
      strerr_diefu1sys(111, "init user name cache") ;
    if (fbf & (1 << PFIELD_GROUP) && !s6ps_grcache_init())
      strerr_diefu1sys(111, "init group name cache") ;
    if (fbf & (1 << PFIELD_TTY) && !s6ps_ttycache_init())
      strerr_diefu1sys(111, "init tty name cache") ;
    if (fbf & (1 << PFIELD_WCHAN) && !s6ps_wchan_init(wchanfile))
    {
      if (wchanfile) strerr_warnwu2sys("init wchan file ", wchanfile) ;
      else strerr_warnwu1sys("init wchan") ;
    }

    {
      size_t fmtpos[n][nfields] ;
      size_t fmtlen[n][nfields] ;
      size_t maxlen[nfields] ;
      unsigned int maxspaces = 0 ;
      for (i = 0 ; i < nfields ; i++) maxlen[i] = strlen(s6ps_fieldheaders[fieldlist[i]]) ;
      for (i = 0 ; i < n ; i++)
      {
        unsigned int j = 0 ;
        for (; j < nfields ; j++)
        {
          if (!(*s6ps_pfield_fmt[fieldlist[j]])(p+i, &fmtpos[i][j], &fmtlen[i][j]))
            strerr_diefu1sys(111, "format fields") ;
          if (fmtlen[i][j] > maxlen[j]) maxlen[j] = fmtlen[i][j] ;
        }
      }
      for (i = 0 ; i < nfields ; i++)
        if (maxlen[i] > maxspaces) maxspaces = maxlen[i] ;
      maxspaces += spacing ;
      if (fbf & (1 << PFIELD_USER)) s6ps_pwcache_finish() ;
      if (fbf & (1 << PFIELD_GROUP)) s6ps_grcache_finish() ;
      if (fbf & (1 << PFIELD_TTY)) s6ps_ttycache_finish() ;
      if (fbf & (1 << PFIELD_WCHAN)) s6ps_wchan_finish() ;
      stralloc_free(&satmp) ;
      {
        char spaces[maxspaces] ;
        for (i = 0 ; i < maxspaces ; i++) spaces[i] = ' ' ;
        for (i = 0 ; i < nfields ; i++)
        {
          unsigned int rightformatted = !!(((uint64_t)1 << fieldlist[i]) & RIGHTFORMATTED) ;
          size_t len = strlen(s6ps_fieldheaders[fieldlist[i]]) ;
          if (rightformatted && (buffer_put(buffer_1, spaces, maxlen[i] - len) < (ssize_t)(maxlen[i] - len)))
            goto nowrite ;
          if (buffer_put(buffer_1, s6ps_fieldheaders[fieldlist[i]], len) < (ssize_t)len)
            goto nowrite ;
          if ((i < nfields-1) && (buffer_put(buffer_1, spaces, !rightformatted * (maxlen[i] - len) + spacing) < (ssize_t)(!rightformatted * (maxlen[i] - len) + spacing)))
            goto nowrite ;
        }
        if (buffer_put(buffer_1, "\n", 1) < 1) goto nowrite ;
        for (i = 0 ; i < n ; i++)
        {
          unsigned int oi = orderedlist[i+1] ;
          unsigned int j = 0 ;
          for (; j < nfields ; j++)
          {
            unsigned int rightformatted = !!(((uint64_t)1 << fieldlist[j]) & RIGHTFORMATTED) ;
            if (rightformatted && (buffer_put(buffer_1, spaces, maxlen[j] - fmtlen[oi][j]) < (ssize_t)(maxlen[j] - fmtlen[oi][j])))
              goto nowrite ;
            if (buffer_put(buffer_1, p[oi].data.s + fmtpos[oi][j], fmtlen[oi][j]) < (ssize_t)fmtlen[oi][j])
              goto nowrite ;
            if ((j < nfields-1) && (buffer_put(buffer_1, spaces, !rightformatted * (maxlen[j] - fmtlen[oi][j]) + spacing) < (ssize_t)(!rightformatted * (maxlen[j] - fmtlen[oi][j]) + spacing)))
              goto nowrite ;
          }
          if (buffer_put(buffer_1, "\n", 1) < 1) goto nowrite ;
        }
      }
    }
  }
  buffer_flush(buffer_1) ;
  return 0 ;

 nowrite:
  strerr_diefu1sys(111, "write to stdout") ;
}

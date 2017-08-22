/* ISC license. */

#include <sys/types.h>
#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <s6-linux-utils/config.h>

#define USAGE "s6-devd [ -q | -v ] [ -b kbufsz ] [ -l linevar ] [ -t maxlife:maxterm:maxkill ] helperprogram..."
#define dieusage() strerr_dieusage(100, USAGE)

static inline int check_targ (char const *s)
{
  size_t pos = 0 ;
  unsigned int t = 0 ;
  pos += uint_scan(s + pos, &t) ;
  if (s[pos] && s[pos++] != ':') return 0 ;
  if (!t) return 1 ;
  pos += uint_scan(s + pos, &t) ;
  if (s[pos] && s[pos++] != ':') return 0 ;
  if (!t) return 1 ;
  pos += uint_scan(s + pos, &t) ;
  if (s[pos]) return 0 ;
  return 1 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  unsigned int kbufsz = 65536, verbosity = 1 ;
  char const *linevar = 0 ;
  char const *targ = 0 ;
  char fmtv[UINT_FMT] ;
  PROG = "s6-devd" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "qvb:l:t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'v' : verbosity++ ; break ;
        case 'b' : if (!uint0_scan(l.arg, &kbufsz)) dieusage() ; break ;
        case 'l' : linevar = l.arg ; break ;
        case 't' : if (!check_targ(l.arg)) dieusage() ; targ = l.arg ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }
  if (!argc) strerr_dieusage(100, USAGE) ;
  if (verbosity != 1) fmtv[uint_fmt(fmtv, verbosity)] = 0 ;

  {
    unsigned int m = 0 ;
    int fd ;
    char const *cargv[argc + 9] ;
    cargv[m++] = S6_LINUX_UTILS_BINPREFIX "s6-uevent-spawner" ;
    if (verbosity != 1)
    {
      cargv[m++] = "-v" ;
      cargv[m++] = fmtv ;
    }
    if (linevar)
    {
      cargv[m++] = "-l" ;
      cargv[m++] = linevar ;
    }
    if (targ)
    {
      cargv[m++] = "-t" ;
      cargv[m++] = targ ;
    }
    cargv[m++] = "--" ;
    while (*argv) cargv[m++] = *argv++ ;
    cargv[m++] = 0 ;
    if (!child_spawn1_pipe(cargv[0], cargv, envp, &fd, 0))
      strerr_diefu2sys(111, "spawn ", cargv[0]) ;
    if (fd_move(1, fd) < 0) strerr_diefu1sys(111, "fd_move") ;
  }

  {
    unsigned int m = 0 ;
    char const *pargv[6] ;
    char fmtk[UINT_FMT] ;
    pargv[m++] = S6_LINUX_UTILS_BINPREFIX "s6-uevent-listener" ;
    if (verbosity != 1)
    {
      pargv[m++] = "-v" ;
      pargv[m++] = fmtv ;
    }
    if (kbufsz != 65536)
    {
      pargv[m++] = "-b" ;
      pargv[m++] = fmtk ;
      fmtk[uint_fmt(fmtk, kbufsz)] = 0 ;
    }
    pargv[m++] = 0 ;
    xpathexec_run(pargv[0], pargv, envp) ;
  }
}

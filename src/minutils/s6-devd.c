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

  {
    size_t pos = 0 ;
    unsigned int m = 0 ;
    char fmt[UINT_FMT * 3] ;
    char const *newargv[argc + 15] ;
    newargv[m++] = S6_LINUX_UTILS_BINPREFIX "s6-uevent-listener" ;
    if (verbosity != 1)
    {
      newargv[m++] = "-v" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, verbosity) ;
      fmt[pos++] = 0 ;
    }
    if (kbufsz != 65536)
    {
      newargv[m++] = "-b" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, kbufsz) ;
      fmt[pos++] = 0 ;
    }
    newargv[m++] = "--" ;
    newargv[m++] = S6_LINUX_UTILS_BINPREFIX "s6-uevent-spawner" ;
    if (verbosity != 1)
    {
      newargv[m++] = "-v" ;
      newargv[m++] = fmt + pos ;
      pos += uint_fmt(fmt + pos, verbosity) ;
      fmt[pos++] = 0 ;
    }
    if (linevar)
    {
      newargv[m++] = "-l" ;
      newargv[m++] = linevar ;
    }
    if (targ)
    {
      newargv[m++] = "-t" ;
      newargv[m++] = targ ;
    }
    newargv[m++] = "--" ;
    while (*argv) newargv[m++] = *argv++ ;
    newargv[m++] = 0 ;
    pathexec_run(newargv[0], newargv, envp) ;
    strerr_dieexec(111, newargv[0]) ;
  }
}

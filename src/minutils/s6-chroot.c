/* ISC license. */

#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <unistd.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-chroot dir prog..."

int main (int argc, char const *const *argv, char const *const *envp)
{
  PROG = "s6-chroot" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  if (chdir(argv[1]) == -1) strerr_diefu2sys(111, "chdir to ", argv[1]) ;
  if (chroot(".") == -1) strerr_diefu2sys(111, "chroot in ", argv[1]) ;
  xpathexec_run(argv[2], argv+2, envp) ;
}

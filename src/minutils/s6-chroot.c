/* ISC license. */

#include <skalibs/nonposix.h>
#include <unistd.h>

#include <skalibs/strerr2.h>
#include <skalibs/exec.h>

#define USAGE "s6-chroot dir prog..."

int main (int argc, char const *const *argv)
{
  PROG = "s6-chroot" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  if (chdir(argv[1]) == -1) strerr_diefu2sys(111, "chdir to ", argv[1]) ;
  if (chroot(".") == -1) strerr_diefu2sys(111, "chroot in ", argv[1]) ;
  xexec(argv+2) ;
}

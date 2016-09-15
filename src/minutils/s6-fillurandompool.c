/* ISC license. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

static int getrandom (void *buf, size_t buflen, unsigned int flags)
{
#ifdef SYS_getrandom
  return syscall(SYS_getrandom, buf, buflen, flags) ;
#else
  return (errno = ENOSYS, -1) ;
#endif
}

int main (void)
{
  char buf[256] ;
  PROG = "s6-fillurandompool" ;
  if (getrandom(buf, 256, 0) != 256)
    strerr_diefu1sys(111, "getrandom") ;
  return 0 ;
}

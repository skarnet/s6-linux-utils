/* ISC license. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <unistd.h>
#include <sys/syscall.h>
#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-fillurandompool"

static int getrandom (void *buf, size_t buflen, unsigned int flags)
{
  return syscall(SYS_getrandom, buf, buflen, flags) ;
}

int main (void)
{
  char buf[256] ;
  PROG = "s6-fillurandompool" ;
  if (getrandom(buf, 256, 0) != 256)
    strerr_diefu1sys(111, "getrandom") ;
  return 0 ;
}

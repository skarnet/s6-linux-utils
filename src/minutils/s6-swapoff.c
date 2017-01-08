/* ISC license. */

#include <sys/types.h>
#include <errno.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

extern int swapoff (char const *) ;

#define USAGE "s6-swapoff device <or> s6-swapoff -a"

#define BUFSIZE 4095

static int swapoffall ( )
{
  char buf[BUFSIZE+1] ;
  buffer b ;
  stralloc sa = STRALLOC_ZERO ;
  int e = 0 ;
  int r ;
  int fd = open_readb("/proc/swaps") ;
  if (fd < 0) strerr_diefu1sys(111, "open_readb /proc/swaps") ;
  buffer_init(&b, &buffer_read, fd, buf, BUFSIZE+1) ;
  if (skagetln(&b, &sa, '\n') < 0) strerr_diefu1sys(111, "skagetln") ;
  for (;;)
  {
    size_t n ;
    sa.len = 0 ;
    r = skagetln(&b, &sa, '\n') ;
    if (r < 0) strerr_diefu1sys(111, "skagetln") ;
    if (!r) break ;
    n = byte_chr(sa.s, sa.len, ' ') ;
    if (n >= sa.len) strerr_dief1x(111, "invalid line in /proc/swaps") ;
    sa.s[n] = 0 ;
    if (swapoff(sa.s) < 0) { e++ ; strerr_warnwu2sys("swapoff ", sa.s) ; }
  }
  fd_close(fd) ;
  stralloc_free(&sa) ;
  return e ;
}

int main (int argc, char const *const *argv)
{
  PROG = "s6-swapoff" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if ((argv[1][0] == '-') && (argv[1][1] == 'a') && !argv[1][2])
    return swapoffall() ;
  if (swapoff(argv[1]) == -1) strerr_diefu2sys(111, "swapoff ", argv[1]) ;
  return 0 ;
}

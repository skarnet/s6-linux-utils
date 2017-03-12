/* ISC license. */

#include <string.h>
#include <sys/mount.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-umount mountpoint <or> s6-umount -a"

#define BUFSIZE 4096
#define MAXLINES 512

static int umountall ( )
{
  stralloc mountpoints[MAXLINES] ;
  char buf[BUFSIZE+1] ;
  buffer b ;
  stralloc sa = STRALLOC_ZERO ;
  unsigned int line = 0 ;
  int e = 0 ;
  int r ;
  int fd = open_readb("/proc/mounts") ;
  if (fd < 0) strerr_diefu1sys(111, "open /proc/mounts") ;
  memset(mountpoints, 0, sizeof(mountpoints)) ;
  buffer_init(&b, &buffer_read, fd, buf, BUFSIZE+1) ;
  for (;;)
  {
    size_t n, p ;
    if (line >= MAXLINES) strerr_dief1x(111, "/proc/mounts too big") ;
    sa.len = 0 ;
    r = skagetln(&b, &sa, '\n') ;
    if (r <= 0) break ;
    p = byte_chr(sa.s, sa.len, ' ') ;
    if (p >= sa.len) strerr_dief1x(111, "bad /proc/mounts format") ;
    p++ ;
    n = byte_chr(sa.s + p, sa.len - p, ' ') ;
    if (n == sa.len - p) strerr_dief1x(111, "bad /proc/mounts format") ;
    if (!stralloc_catb(&mountpoints[line], sa.s + p, n) || !stralloc_0(&mountpoints[line]))
      strerr_diefu1sys(111, "store mount point") ;
    line++ ;
  }
  fd_close(fd) ;
  stralloc_free(&sa) ;
  if (r < 0) strerr_diefu1sys(111, "read /proc/mounts") ;
  while (line--)
    if (umount(mountpoints[line].s) == -1)
    {
      e++ ;
      strerr_warnwu2sys("umount ", mountpoints[line].s) ;
    }
  return e ;
}

int main (int argc, char const *const *argv)
{
  PROG = "s6-umount" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if ((argv[1][0] == '-') && (argv[1][1] == 'a') && !argv[1][2])
    return umountall() ;
  if (umount(argv[1]) == -1) strerr_diefu2sys(111, "umount ", argv[1]) ;
  return 0 ;
}

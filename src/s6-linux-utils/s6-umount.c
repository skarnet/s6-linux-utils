/* ISC license. */

#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

#include <skalibs/uint64.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/envexec.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-umount [ -a ] [ mountpoint ]"

#define UMOUNTALL_MAXLINES 512

static int umountall (void)
{
  stralloc mountpoints[UMOUNTALL_MAXLINES] ;
  char buf[4096] ;
  buffer b ;
  stralloc sa = STRALLOC_ZERO ;
  unsigned int line = 0 ;
  int e = 0 ;
  int r ;
  int fd = open_readb("/proc/mounts") ;
  if (fd < 0) strerr_diefu1sys(111, "open /proc/mounts") ;
  memset(mountpoints, 0, sizeof(mountpoints)) ;
  buffer_init(&b, &buffer_read, fd, buf, 4096) ;
  for (;;)
  {
    size_t n, p ;
    if (line >= UMOUNTALL_MAXLINES)
      strerr_dief1x(111, "/proc/mounts too big") ;
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
  static gol_bool const rgolb[] =
  {
    { .so = 'a', .lo = "all", .clear = 0, .set = 1 },
  } ;
  uint64_t wgolb = 0 ;
  unsigned int golc ;
  PROG = "s6-umount" ;
  golc = gol_main(argc, argv, rgolb, 1, 0, 0, &wgolb, 0) ;
  argc -= golc ; argv += golc ;
  if (wgolb & 1) _exit(umountall()) ;
  if (!argc) strerr_dieusage(100, USAGE) ;
  if (umount(argv[0]) == -1) strerr_diefu2sys(111, "unmount ", argv[0]) ;
  _exit(0) ;
}

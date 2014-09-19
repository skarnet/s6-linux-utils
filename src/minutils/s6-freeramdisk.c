/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <skalibs/strerr2.h>

#define USAGE "s6-freeramdisk ramdisk_device"

int main (int argc, char const *const *argv)
{
  int fd ;
  PROG = "s6-freeramdisk" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  fd = open(argv[1], O_RDWR) ;
  if (fd < 0) strerr_diefu3sys(111, "open ", argv[1], " in read-write mode") ;
  if (ioctl(fd, BLKFLSBUF) < 0) strerr_diefu2sys(111, "ioctl ", argv[1]) ;
  return 0 ;
}

/* ISC license. */

#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <mntent.h>
#include <stdio.h>
#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include "mount-constants.h"

#define USAGE "s6-mount -a [ -z fstab ] | s6-mount [ -n ] [ -t type ] [ -o option[,option...] ]... device mountpoint"
#define BUFSIZE 4096

#define SWITCH(opt) do
#define HCTIWS(opt) while(0) ;
#define CASE(s) if (n == sizeof(s) - 1 && !strncmp(opt, (s), n))

static void scanopt (stralloc *data, unsigned long *flags, char const *opt)
{
  for (;;)
  {
    unsigned int n = str_chr(opt, ',') ;
    SWITCH(opt)
    {
      CASE("defaults") { *flags = MS_MGC_VAL ; break ; }
      CASE("ro") { *flags |= MS_RDONLY ; break ; }
      CASE("rw") { *flags &= ~MS_RDONLY ; break ; }
      CASE("remount") { *flags |= MS_REMOUNT ; break ; }
      CASE("sync") { *flags |= MS_SYNCHRONOUS ; break ; }
      CASE("async") { *flags &= ~MS_SYNCHRONOUS ; break ; }
      CASE("nodev") { *flags |= MS_NODEV ; break ; }
      CASE("dev") { *flags &= ~MS_NODEV ; break ; }
      CASE("noexec") { *flags |= MS_NOEXEC ; break ; }
      CASE("exec") { *flags &= ~MS_NOEXEC ; break ; }
      CASE("nosuid") { *flags |= MS_NOSUID ; break ; }
      CASE("suid") { *flags &= ~MS_NOSUID ; break ; }
      CASE("noatime") { *flags |= MS_NOATIME ; break ; }
      CASE("atime") { *flags &= ~MS_NOATIME ; break ; }
      CASE("nodiratime") { *flags |= MS_NODIRATIME ; break ; }
      CASE("diratime") { *flags &= ~MS_NODIRATIME ; break ; }
      CASE("strictatime") { *flags |= MS_STRICTATIME ; break ; }
      CASE("nostrictatime") { *flags &= ~MS_STRICTATIME ; break ; }
      CASE("bind") { *flags |= MS_BIND ; break ; }
      CASE("nobind") { *flags &= ~MS_BIND ; break ; }
      CASE("move") { *flags |= MS_MOVE ; break ; }
      CASE("nomove") { *flags &= ~MS_MOVE ; break ; }
      CASE("dirsync") { *flags |= MS_DIRSYNC ; break ; }
      CASE("nodirsync") { *flags &= ~MS_DIRSYNC ; break ; }
      CASE("mandlock") { *flags |= MS_MANDLOCK ; break ; }
      CASE("nomandlock") { *flags &= ~MS_MANDLOCK ; break ; }
      CASE("silent") { *flags |= MS_SILENT ; break ; }
      CASE("nosilent") { *flags &= ~MS_SILENT ; break ; }
#ifdef MS_LAZYTIME
      CASE("lazytime") { *flags |= MS_LAZYTIME ; break ; }
      CASE("nolazytime") { *flags &= ~MS_LAZYTIME ; break ; }
#endif

      CASE("shared") { *flags &= ~(MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE) ; *flags |= MS_SHARED ; break ; }
      CASE("private") { *flags &= ~(MS_SHARED | MS_SLAVE | MS_UNBINDABLE) ; *flags |= MS_PRIVATE ; break ; }
      CASE("slave") { *flags &= ~(MS_SHARED | MS_PRIVATE | MS_UNBINDABLE) ; *flags |= MS_SLAVE ; break ; }
      CASE("unbindable") { *flags &= ~(MS_SHARED | MS_PRIVATE | MS_SLAVE) ; *flags |= MS_UNBINDABLE ; break ; }

        if ((data->s && data->len && !stralloc_catb(data, ",", 1)) || !stralloc_catb(data, opt, n))
          strerr_diefu1sys(111, "build data string") ;
    }
    HCTIWS(opt)

    opt += n ;
    if (!*opt) break ;
    if (*opt != ',') strerr_dief1x(100, "unrecognized option") ;
    opt++ ;
  }
}

static int mountall (char const *fstab)
{
  struct mntent *d ;
  int e = 0 ;
  FILE *yuck = setmntent(fstab, "r") ;
  if (!yuck) strerr_diefu2sys(111, "open ", fstab) ;
  while ((d = getmntent(yuck)))
  {
    unsigned long flags = MS_MGC_VAL ;
    stralloc data = STRALLOC_ZERO ;
    scanopt(&data, &flags, d->mnt_opts) ;
    if (!stralloc_0(&data))
      strerr_diefu1sys(111, "build data string") ;
#ifdef DEBUG
    strerr_warni4x("mounting ", d->mnt_fsname, " on ", d->mnt_dir) ;
#endif
    if (mount(d->mnt_fsname, d->mnt_dir, d->mnt_type, flags, data.s) == -1)
    {
      e++ ;
      strerr_warnwu4sys("mount ", d->mnt_fsname, " on ", d->mnt_dir) ;
    }
    stralloc_free(&data) ;
  }
  endmntent(yuck) ;
  return e ;
}

int main (int argc, char const *const *argv)
{
  stralloc data = STRALLOC_ZERO ;
  unsigned long flags = MS_MGC_VAL ;
  char const *fstype = "none" ;
  char const *fstab = "/etc/fstab" ;
  PROG = "s6-mount" ;
  {
    int doall = 0 ;
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "nz:arwt:o:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'n' : break ;
        case 'z' : fstab = l.arg ; break ;
        case 'a' : doall = 1 ; break ;
        case 't' : fstype = l.arg ; break ;
        case 'w' : scanopt(&data, &flags, "rw") ; break ;
        case 'r' : scanopt(&data, &flags, "ro") ; break ;
        case 'o' : scanopt(&data, &flags, l.arg) ; break ;
        default : strerr_dieusage(100, USAGE) ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (doall) return mountall(fstab) ;
  }
  if (!argc)
  {
    int fd = open_readb("/proc/mounts") ;
    if (fd < 0) strerr_diefu2sys(111, "read ", "/proc/mounts") ;
    if (fd_cat(fd, 1) < 0) strerr_diefu2sys(111, "fd_cat ", "/proc/mounts") ;
    fd_close(fd) ;
  }
  else if (argc == 1) strerr_dieusage(100, USAGE) ;
  else if (!stralloc_0(&data)) strerr_diefu1sys(111, "build data string") ;  
  else if (mount(argv[0], argv[1], fstype, flags, data.s) == -1)
    strerr_diefu4sys(errno == EBUSY ? 1 : 111, "mount ", argv[0], " on ", argv[1]) ;
  return 0 ;
}

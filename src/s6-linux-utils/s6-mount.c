/* ISC license. */

#include <string.h>
#include <errno.h>
#include <sys/mount.h>
#include <mntent.h>
#include <stdio.h>
#include <stdlib.h>

#include <skalibs/bytestr.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#include "mount-constants.h"

#define USAGE "s6-mount -a [ -z fstab ] | s6-mount [ -n ] [ -t type ] [ -o option[,option...] ]... device mountpoint"
#define dienomem() strerr_diefu1sys(111, "stralloc_catb")

struct mount_flag_modif_s
{
  char const *name ;
  unsigned long add ;
  unsigned long del ;
} ;

static int mount_flag_cmp (void const *a, void const *b)
{
  char const *key = a ;
  struct mount_flag_modif_s const *p = b ;
  return strcmp(key, p->name) ;
}

static void mount_scanopt (stralloc *data, unsigned long *flags, char const *s)
{
  static struct mount_flag_modif_s const mount_flag_modifs[] =
  {
    { .name = "async", .add = 0, .del = MS_SYNCHRONOUS },
    { .name = "atime", .add = 0, .del = MS_NOATIME },
    { .name = "bind", .add = MS_BIND, .del = 0 },
    { .name = "defaults", .add = 0, .del = MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_SYNCHRONOUS },
    { .name = "dev", .add = 0, .del = MS_NODEV },
    { .name = "diratime", .add = 0, .del = MS_NODIRATIME },
    { .name = "dirsync", .add = MS_DIRSYNC, .del = 0 },
    { .name = "exec", .add = 0, .del = MS_NOEXEC  },
    { .name = "lazytime", .add = MS_LAZYTIME, .del = 0 },
    { .name = "loud", .add = 0, .del = MS_SILENT },
    { .name = "mandlock", .add = MS_MANDLOCK, .del = 0 },
    { .name = "move", .add = MS_MOVE, .del = 0 },
    { .name = "noatime", .add = MS_NOATIME, .del = 0 },
    { .name = "nobind", .add = 0, .del = MS_BIND },
    { .name = "nodev", .add = MS_NODEV, .del = 0 },
    { .name = "nodiratime", .add = MS_NODIRATIME, .del = 0 },
    { .name = "nodirsync", .add = 0, .del = MS_DIRSYNC },
    { .name = "nolazytime", .add = 0, .del = MS_LAZYTIME },
    { .name = "noexec", .add = MS_NOEXEC, .del = 0 },
    { .name = "nomandlock", .add = 0, .del = MS_MANDLOCK },
    { .name = "nomove", .add = 0, .del = MS_MOVE },
    { .name = "norelatime", .add = 0, .del = MS_RELATIME },
    { .name = "nostrictatime", .add = 0, .del = MS_STRICTATIME },
    { .name = "nosymfollow", .add = MS_NOSYMFOLLOW, .del = 0 },
    { .name = "nosuid", .add = MS_NOSUID, .del = 0 },
    { .name = "private", .add = MS_PRIVATE, .del = MS_SHARED | MS_SLAVE | MS_UNBINDABLE },
    { .name = "relatime", .add = MS_RELATIME, .del = 0 },
    { .name = "remount", .add = MS_REMOUNT, .del = 0 },
    { .name = "ro", .add = MS_RDONLY, .del = 0 },
    { .name = "rw", .add = 0, .del = MS_RDONLY },
    { .name = "shared", .add = MS_SHARED, .del = MS_PRIVATE | MS_SLAVE | MS_UNBINDABLE },
    { .name = "silent", .add = MS_SILENT, .del = 0 },
    { .name = "slave", .add = MS_SLAVE, .del = MS_SHARED | MS_PRIVATE | MS_UNBINDABLE },
    { .name = "strictatime", .add = MS_STRICTATIME, .del = 0 },
    { .name = "suid", .add = 0, .del = MS_NOSUID },
    { .name = "symfollow", .add = 0, .del = MS_NOSYMFOLLOW },
    { .name = "sync", .add = MS_SYNCHRONOUS, .del = 0 },
    { .name = "unbindable", .add = MS_UNBINDABLE, .del = MS_SHARED | MS_PRIVATE | MS_SLAVE },
  } ;

  while (*s)
  {
    struct mount_flag_modif_s const *p ;
    size_t n = str_chr(s, ',') ;
    char opt[n+1] ;
    memcpy(opt, s, n) ;
    opt[n] = 0 ;
    p = bsearch(opt, mount_flag_modifs, sizeof(mount_flag_modifs) / sizeof(struct mount_flag_modif_s), sizeof(struct mount_flag_modif_s), &mount_flag_cmp) ;
    if (p)
    {
      *flags &= ~p->del ;
      *flags |= p->add ;
    }
    else
    {
      if (data->s && data->len && !stralloc_catb(data, ",", 1)) dienomem() ;
      if (!stralloc_catb(data, s, n)) dienomem() ;
    }
    s += n + (s[n] == ',') ;
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
    unsigned long flags = 0 ;
    stralloc data = STRALLOC_ZERO ;
    mount_scanopt(&data, &flags, d->mnt_opts) ;
    if (!stralloc_0(&data))
      strerr_diefu1sys(111, "build data string") ;
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
  unsigned long flags = 0 ;
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
        case 'w' : mount_scanopt(&data, &flags, "rw") ; break ;
        case 'r' : mount_scanopt(&data, &flags, "ro") ; break ;
        case 'o' : mount_scanopt(&data, &flags, l.arg) ; break ;
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

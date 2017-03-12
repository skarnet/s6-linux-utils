/* ISC license. */

#include <unistd.h>
#include <sys/swap.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <skalibs/strerr2.h>

#define USAGE "s6-swapon device <or> s6-swapon -a"

static int swaponall ()
{
  struct mntent *d ;
  int e = 0 ;
  FILE *yuck = setmntent("/etc/fstab", "r") ;
  if (!yuck) strerr_diefu1sys(111, "setmntent /etc/fstab") ;
  while ((d = getmntent(yuck)))
    if (!strcmp(d->mnt_type, "swap") && (swapon(d->mnt_fsname, 0) == -1))
    {
      e++ ;
      strerr_warnwu2sys("swapon ", d->mnt_fsname) ;
    }
  endmntent(yuck) ;
  return e ;
}

int main (int argc, char const *const *argv)
{
  PROG = "s6-swapon" ;
  if (argc < 2) strerr_dieusage(100, USAGE) ;
  if ((argv[1][0] == '-') && (argv[1][1] == 'a') && !argv[1][2])
    return swaponall() ;
  if (swapon(argv[1], 0) == -1)
    strerr_diefu2sys(111, "swapon ", argv[1]) ;
  return 0 ;
}

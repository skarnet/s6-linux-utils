/* ISC license. */

#include <unistd.h>
#include <sys/swap.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>

#include <skalibs/uint64.h>
#include <skalibs/envexec.h>

#define USAGE "s6-swapon [ -a ] [ device ]"

static int swaponall (void)
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
  static gol_bool const rgolb[] =
  {
    { .so = 'a', .lo = "all", .clear = 0, .set = 1 },
  } ;
  uint64_t wgolb = 0 ;
  unsigned int golc ;
  PROG = "s6-swapon" ;
  golc = gol_main(argc, argv, rgolb, 1, 0, 0, &wgolb, 0) ;
  argc -= golc ; argv += golc ;
  if (wgolb & 1) _exit(swaponall()) ;
  if (!argc) strerr_dieusage(100, USAGE) ;
  if (swapon(argv[0], 0) == -1) strerr_diefu2sys(111, "swapon ", argv[0]) ;
  _exit(0) ;
}

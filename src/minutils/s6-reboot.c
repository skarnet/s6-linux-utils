/* ISC license. */

#include <unistd.h>
#include <sys/reboot.h>
#include <skalibs/strerr2.h>

int main ()
{
  PROG = "s6-reboot" ;
  sync() ;
  reboot(RB_AUTOBOOT) ;
  strerr_diefu1sys(111, "reboot()") ;
}

/* ISC license. */

#include <unistd.h>
#include <sys/reboot.h>
#include <skalibs/strerr2.h>

int main ()
{
  PROG = "s6-poweroff" ;
  sync() ;
  reboot(RB_POWER_OFF) ;
  strerr_diefu1sys(111, "reboot()") ;
}

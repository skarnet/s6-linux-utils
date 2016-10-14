/* ISC license. */

#include <skalibs/strerr2.h>
#include <skalibs/random.h>

int main (void)
{
  char c ;
  PROG = "s6-fillurandompool" ;
  if (!random_init())
    strerr_diefu1sys(111, "initialize random generator") ;
  random_string(&c, 1) ;
  return 0 ;
}

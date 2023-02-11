/* ISC license. */

#include <skalibs/nonposix.h>

#include <unistd.h>
#include <string.h>

#include <skalibs/allreadwrite.h>
#include <skalibs/strerr.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>

#define USAGE "s6-hostname [ hostname ]"

static int hostname_getit (void)
{
  stralloc sa = STRALLOC_ZERO ;
  if (sagethostname(&sa) < 0) strerr_diefu1sys(111, "get hostname") ;
  sa.s[sa.len++] = '\n' ;
  if (allwrite(1, sa.s, sa.len) < sa.len)
    strerr_diefu1sys(111, "write to stdout") ;
  return 0 ;
}

static int hostname_setit (char const *h)
{
  if (sethostname(h, strlen(h)) < 0)
    strerr_diefu1sys(111, "set hostname") ;
  return 0 ;
}

int main (int argc, char const *const *argv)
{
  PROG = "s6-hostname" ;
  return (argc < 2) ? hostname_getit() : hostname_setit(argv[1]) ;
}

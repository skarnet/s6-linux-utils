/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <spawn.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <skalibs/config.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/env.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-uevent-spawner [ -v verbosity ] [ -l linevar ] [ -t maxlife:maxterm:maxkill ] helperprogram..."
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "build string") ;

static unsigned int cont = 1, state = 0, verbosity = 1 ;
static pid_t pid = 0 ;
static tain_t lifetto = TAIN_INFINITE_RELATIVE,
              termtto = TAIN_INFINITE_RELATIVE,
              killtto = TAIN_INFINITE_RELATIVE,
              deadline ;

static inline void on_death (void)
{
  pid = 0 ;
  state = 0 ;
  tain_add_g(&deadline, &tain_infinite_relative) ;
}

static inline void on_event (char const *const *argv, char const *const *envp, char const *s, unsigned int len)
{
  posix_spawnattr_t attr ;
  posix_spawn_file_actions_t actions ;
  unsigned int envlen = env_len(envp) ;
  unsigned int n = envlen + 1 + byte_count(s, len, '\0') ;
  pid_t mypid ;
  int e ;
  char const *v[n] ;
  if (!env_merge(v, n, envp, envlen, s, len))
    strerr_diefu1sys(111, "env_merge") ;

  e = posix_spawnattr_init(&attr) ;
  if (e) { errno = e ; strerr_diefu1sys(111, "posix_spawnattr_init") ; }
  {
    sigset_t set ;
    sigemptyset(&set) ;
    e = posix_spawnattr_setsigmask(&attr, &set) ;
    if (e) { errno = e ; strerr_diefu1sys(111, "posix_spawnattr_setsigmask") ; }
    sigfillset(&set) ;
    e = posix_spawnattr_setsigdefault(&attr, &set) ;
    if (e) { errno = e ; strerr_diefu1sys(111, "posix_spawnattr_setsigdefault") ; }
  }
  e = posix_spawn_file_actions_init(&actions) ;
  if (e) { errno = e ; strerr_diefu1sys(111, "posix_spawn_file_actions_init") ; }
  e = posix_spawn_file_actions_addopen(&actions, 0, "/dev/null", O_RDONLY, S_IRUSR) ;
  if (e) { errno = e ; strerr_diefu1sys(111, "posix_spawn_file_actions_addopen") ; }
  e = posix_spawnp(&mypid, argv[0], &actions, &attr, (char *const *)argv, (char * const *)v) ;
  if (e) { errno = e ; strerr_diefu2sys(111, "spawn ", argv[0]) ; }
  posix_spawn_file_actions_destroy(&actions) ;
  posix_spawnattr_destroy(&attr) ;
  state = 1 ;
  pid = mypid ;
  tain_add_g(&deadline, &lifetto) ;
}

static inline void handle_timeout (void)
{
  switch (state)
  {
    case 0 :
      tain_add_g(&deadline, &tain_infinite_relative) ;
      break ;
    case 1 :
      kill(pid, SIGTERM) ;
      tain_add_g(&deadline, &termtto) ;
      state++ ;
      break ;
    case 2 :
      kill(pid, SIGKILL) ;
      tain_add_g(&deadline, &killtto) ;
      state++ ;
      break ;
    case 3 :
      strerr_dief1x(99, "child resisted SIGKILL - check your kernel logs.") ;
    default :
      strerr_dief1x(101, "internal error: inconsistent state. Please submit a bug-report.") ;
  }
}

static inline void handle_signals (void)
{
  for (;;)
  {
    int c = selfpipe_read() ;
    switch (c)
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGCHLD :
        if (!pid) wait_reap() ;
        else
        {
          int wstat ;
          int r = wait_pid_nohang(pid, &wstat) ;
          if (r < 0)
            if (errno != ECHILD) strerr_diefu1sys(111, "wait_pid_nohang") ;
            else break ;
          else if (!r) break ;
          on_death() ;
        }
        break ;
      default :
        strerr_dief1x(101, "internal error: inconsistent signal handling. Please submit a bug-report.") ;
    }
  }
}

static inline void handle_stdin (stralloc *sa, char const *linevar, char const *const *argv, char const *const *envp)
{
  while (!pid)
  {
    unsigned int start ;
    register int r ;
    if (!sa->len && linevar)
      if (!stralloc_cats(sa, linevar) || !stralloc_catb(sa, "=", 1))
        dienomem() ;
    start = sa->len ;
    r = sanitize_read(skagetln(buffer_0, sa, 0)) ;
    if (r < 0)
    {
      cont = 0 ;
      if (errno != EPIPE && verbosity) strerr_warnwu1sys("read from stdin") ;
    }
    if (r <= 0) break ;
    if (sa->len == start + 1)
    {
      start = linevar ? 0 : str_len(sa->s) + 1 ;
      if (start >= sa->len)
      {
        if (verbosity) strerr_warnw1x("read an empty event!") ;
      }
      else on_event(argv, envp, sa->s + start, sa->len - 1 - start) ;
      sa->len = 0 ;
    }
  }
}

static inline int make_ttos (char const *s)
{
  unsigned int tlife = 0, tterm = 0, tkill = 0, pos = 0 ;
  pos += uint_scan(s + pos, &tlife) ;
  if (s[pos] && s[pos++] != ':') return 0 ;
  if (!tlife) return 1 ;
  tain_from_millisecs(&lifetto, tlife) ;
  pos += uint_scan(s + pos, &tterm) ;
  if (s[pos] && s[pos++] != ':') return 0 ;
  if (!tterm) return 1 ;
  tain_from_millisecs(&termtto, tterm) ;
  tain_add(&termtto, &termtto, &lifetto) ;
  pos += uint_scan(s + pos, &tkill) ;
  if (s[pos]) return 0 ;
  if (!tkill) return 1 ;
  tain_from_millisecs(&killtto, tkill) ;
  tain_add(&killtto, &killtto, &termtto) ;
  return 1 ;
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .fd = 0, .events = IOPAUSE_READ } } ;
  char const *linevar = 0 ;
  stralloc sa = STRALLOC_ZERO ;
  PROG = "s6-uevent-spawner" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "l:v:t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'l' : linevar = l.arg ; break ;
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 't' : if (!make_ttos(l.arg)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) strerr_dieusage(100, USAGE) ;
  }
  if (linevar && linevar[str_chr(linevar, '=')])
    strerr_dief2x(100, "invalid variable: ", linevar) ;

  if (ndelay_on(0) < 0) strerr_diefu1sys(111, "set stdin nonblocking") ;
  x[0].fd = selfpipe_init() ;
  if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  if (selfpipe_trap(SIGCHLD) < 0) strerr_diefu1sys(111, "trap SIGCHLD") ;
  if (setenv("PATH", SKALIBS_DEFAULTPATH, 0) < 0)
    strerr_diefu1sys(111, "setenv PATH") ;

  tain_now_g() ;
  tain_add_g(&deadline, &tain_infinite_relative) ;
  if (verbosity >= 2) strerr_warni1x("starting") ;

  while (cont || pid)
  {
    register int r ;
    if (buffer_len(buffer_0))
      handle_stdin(&sa, linevar, argv, envp) ;
    r = iopause_g(x, 1 + (!pid && cont), &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r) handle_timeout() ;
    else
    {
      if (x[0].revents & IOPAUSE_EXCEPT)
        strerr_diefu1x(111, "iopause: trouble with selfpipe") ;
      if (x[0].revents & IOPAUSE_READ)
        handle_signals() ;
      else if (cont && !pid && x[1].revents & IOPAUSE_READ)
        handle_stdin(&sa, linevar, argv, envp) ;
    }
  }
  if (verbosity >= 2) strerr_warni1x("exiting") ;
  return 0 ;
}

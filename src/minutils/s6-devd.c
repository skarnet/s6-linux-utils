/* ISC license. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <unistd.h>
#include <spawn.h>
#include <signal.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <errno.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>

#define USAGE "s6-devd [ -q | -v ] [ -b kbufsz ] [ -t maxlife:maxterm:maxkill ] helperprogram..."
#define dieusage() strerr_dieusage(100, USAGE)

static unsigned int cont = 1, state = 0, verbosity = 1 ;
static pid_t pid ;
static tain_t lifetto = TAIN_INFINITE_RELATIVE,
              termtto = TAIN_INFINITE_RELATIVE,
              killtto = TAIN_INFINITE_RELATIVE,
              deadline ;

static inline int fd_recvmsg (int fd, struct msghdr *hdr)
{
  int r ;
  do r = recvmsg(fd, hdr, 0) ;
  while ((r == -1) && (errno == EINTR)) ;
  return r ;
}

static inline int netlink_init (unsigned int kbufsz)
{
  struct sockaddr_nl nl = { .nl_family = AF_NETLINK, .nl_pad = 0, .nl_groups = 1, .nl_pid = 0 } ;
  int fd = socket_internal(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT, DJBUNIX_FLAG_NB|DJBUNIX_FLAG_COE) ;
  if (fd < 0) return -1 ;
  if (bind(fd, (struct sockaddr *)&nl, sizeof(struct sockaddr_nl)) < 0) goto err ;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &kbufsz, sizeof(unsigned int)) < 0) goto err ;
  return fd ;
 err:
  {
    register int e = errno ;
    fd_close(fd) ;
    errno = e ;
  }
  return -1 ;
}

static inline void on_death (void)
{
  pid = 0 ;
  state = 0 ;
  tain_add_g(&deadline, &tain_infinite_relative) ;
  if (cont == 2) cont = 0 ;
}

static inline void on_event (char const *const *argv, char const *const *envp, char const *s, unsigned int len)
{
  unsigned int envlen = env_len(envp) ;
  unsigned int n = envlen + 1 + byte_count(s, len, '\0') ;
  int e ;
  char const *v[n] ;
  if (!env_merge(v, n, envp, envlen, s, len))
    strerr_diefu1sys(111, "env_merge") ;
  e = posix_spawnp(&pid, argv[0], 0, 0, (char *const *)argv, (char * const *)v) ;
  if (e) { errno = e ; strerr_diefu2sys(111, "spawn ", argv[0]) ; }
  state = 1 ;
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
    char c = selfpipe_read() ;
    switch (c)
    {
      case -1 : strerr_diefu1sys(111, "selfpipe_read") ;
      case 0 : return ;
      case SIGTERM :
        cont = pid ? 2 : 0 ;
        break ;
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
        strerr_dief1x(101, "internal error: inconsistent signal state. Please submit a bug-report.") ;
    }
  }
}

static inline void handle_netlink (int fd, char const *const *argv, char const *const *envp)
{
  char buf[4096] ;
  int r ;
  {
    struct sockaddr_nl nl;
    struct iovec iov = { .iov_base = &buf, .iov_len = sizeof(buf) } ;
    char ctlmsg[CMSG_SPACE(sizeof(struct ucred))] ;
    struct msghdr msg = {
                          .msg_name = &nl,
                          .msg_namelen = sizeof(struct sockaddr_nl),
                          .msg_iov = &iov,
                          .msg_iovlen = 1,
                          .msg_control = ctlmsg,
                          .msg_controllen = sizeof(ctlmsg),
                          .msg_flags = 0
                        } ;
    r = sanitize_read(fd_recvmsg(fd, &msg)) ;
    if (r < 0)
    {
      if (errno == EPIPE)
      {
        if (verbosity >= 2) strerr_warnw1x("received EOF on netlink") ;
        cont = 0 ;
        return ;
      }
      else strerr_diefu1sys(111, "receive netlink message") ;
    }
    if (!r) return ;
    if (r < 32 || r > 4096)
    {
      if (verbosity >= 2)
        strerr_warnw2x("received and ignored netlink message ", "with invalid length") ;
      return ;
    }
    if (nl.nl_pid)
    {
      if (verbosity >= 3)
      {
        char fmt[UINT_FMT] ;
        fmt[uint_fmt(fmt, nl.nl_pid)] = 0 ;
        strerr_warnw3x("received and ignored netlink message ", "from userspace process ", fmt) ;
      }
      return ;
    }
  }
  {
    unsigned int start = str_len(buf) + 1 ;
    if (start < 5 || start > (unsigned int)r)
    {
      if (verbosity >= 2)
        strerr_warnw3x("received and ignored netlink message ", "with invalid header", " length") ;
      return ;
    }
    if (str_strn(buf, start, "@/", 2) >= start)
    {
      if (verbosity >= 2)
        strerr_warnw2x("received and ignored netlink message ", "with invalid header") ;
      return ;
    }
    on_event(argv, envp, buf + start, r - start) ;
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
  iopause_fd x[2] = { { -1, IOPAUSE_READ, 0 }, { -1, IOPAUSE_READ, 0 } } ;
  PROG = "s6-devd" ;
  {
    unsigned int kbufsz = 65536 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "qvb:t:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'q' : if (verbosity) verbosity-- ; break ;
        case 'v' : verbosity++ ; break ;
        case 'b' : if (!uint0_scan(l.arg, &kbufsz)) dieusage() ; break ;
        case 't' : if (!make_ttos(l.arg)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) strerr_dieusage(100, USAGE) ;
    close(0) ;
    if (open_readb("/dev/null") < 0) strerr_diefu1sys(111, "open /dev/null for reading") ;
    x[1].fd = netlink_init(kbufsz) ;
    if (x[1].fd < 0) strerr_diefu1sys(111, "init netlink") ;
  }

  x[0].fd = selfpipe_init() ;
  if (x[0].fd == -1) strerr_diefu1sys(111, "init selfpipe") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGCHLD) ;
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }

  tain_now_g() ;
  tain_add_g(&deadline, &tain_infinite_relative) ;
  if (verbosity >= 2) strerr_warni1x("starting") ;

  while (cont)
  {
    register int r = iopause_g(x, 1 + !pid, &deadline) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    else if (!r) handle_timeout() ;
    else
    {
      if ((x[0].revents | x[1].revents) & IOPAUSE_EXCEPT)
        strerr_diefu1x(111, "iopause: trouble with pipes") ;
      if (x[0].revents & IOPAUSE_READ) handle_signals() ;
      else if (!pid && (x[1].revents & IOPAUSE_READ))
        handle_netlink(x[1].fd, argv, envp) ;
    }
  }
  if (verbosity >= 2) strerr_warni1x("exiting") ;
  return 0 ;
}

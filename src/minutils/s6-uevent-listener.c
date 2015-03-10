/* ISC license. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <linux/netlink.h>
#include <skalibs/uint.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/siovec.h>
#include <skalibs/buffer.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>

#define USAGE "s6-uevent-listener [ -v verbosity ] [ -b kbufsz ] helperprogram..."
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "build string") ;

#define MAXNLSIZE 4096
#define BUFSIZE 8191

static char buf1[BUFSIZE + 1] ;
static buffer b1 = BUFFER_INIT(&fd_writesv, 1, buf1, BUFSIZE + 1) ;
static unsigned int cont = 1, verbosity = 1 ;
static pid_t pid ;

static inline int fd_recvmsg (int fd, struct msghdr *hdr)
{
  int r ;
  do r = recvmsg(fd, hdr, MSG_DONTWAIT) ;
  while ((r == -1) && (errno == EINTR)) ;
  return r ;
}

static inline int netlink_init_stdin (unsigned int kbufsz)
{
  struct sockaddr_nl nl = { .nl_family = AF_NETLINK, .nl_pad = 0, .nl_groups = 1, .nl_pid = 0 } ;
  close(0) ;
  return socket_internal(AF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT, DJBUNIX_FLAG_NB|DJBUNIX_FLAG_COE) == 0
   && bind(0, (struct sockaddr *)&nl, sizeof(struct sockaddr_nl)) == 0
   && setsockopt(0, SOL_SOCKET, SO_RCVBUFFORCE, &kbufsz, sizeof(unsigned int)) == 0 ;
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
        cont = 0 ;
        fd_close(0) ;
        break ;
      case SIGCHLD :
      {
        char fmt[UINT_FMT] ;
        int wstat ;
        int r = wait_pid_nohang(pid, &wstat) ;
        if (r < 0)
          if (errno != ECHILD) strerr_diefu1sys(111, "wait_pid_nohang") ;
          else break ;
        else if (!r) break ;
        if (WIFSIGNALED(wstat))
        {
          fmt[uint_fmt(fmt, WTERMSIG(wstat))] = 0 ;
          strerr_dief2x(1, "child crashed with signal ", fmt) ;
        }
        else
        {
          fmt[uint_fmt(fmt, WEXITSTATUS(wstat))] = 0 ;
          strerr_dief2x(1, "child exited ", fmt) ;
        }
      }
      default :
        strerr_dief1x(101, "internal error: inconsistent signal state. Please submit a bug-report.") ;
    }
  }
}

static inline void handle_stdout (void)
{
  if (!buffer_flush(&b1)) strerr_diefu1sys(111, "flush stdout") ;
}

static inline void handle_netlink (void)
{
  struct sockaddr_nl nl;
  struct iovec iov[2] ;
  struct msghdr msg =
  {
    .msg_name = &nl,
    .msg_namelen = sizeof(struct sockaddr_nl),
    .msg_iov = iov,
    .msg_iovlen = 2,
    .msg_control = 0,
    .msg_controllen = 0,
    .msg_flags = 0
  } ;
  siovec_t v[2] ;
  register int r ;
  buffer_wpeek(&b1, v) ;
  siovec_trunc(v, 2, siovec_len(v, 2) - 1) ;
  iovec_from_siovec(iov, v, 2) ;
  r = sanitize_read(fd_recvmsg(0, &msg)) ;
  if (r < 0)
  {
    if (errno == EPIPE)
    {
      if (verbosity >= 2) strerr_warnw1x("received EOF on netlink") ;
      cont = 0 ;
      fd_close(0) ;
      return ;
    }
    else strerr_diefu1sys(111, "receive netlink message") ;
  }
  if (!r) return ;
  if (msg.msg_flags & MSG_TRUNC)
    strerr_diefu2x(111, "buffer too small for ", "netlink message") ;
  if (nl.nl_pid)
  {
    if (verbosity >= 3)
    {
      char fmt[UINT_FMT] ;
      fmt[uint_fmt(fmt, nl.nl_pid)] = 0 ;
      strerr_warnw3x("netlink message", " from userspace process ", fmt) ;
    }
    return ;
  }
  buffer_wseek(&b1, r) ;
  buffer_putnoflush(&b1, "", 1) ;
}


int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[3] = { { .events = IOPAUSE_READ }, { .fd = 1 }, { .fd = 0 } } ;
  PROG = "s6-uevent-listener" ;
  {
    unsigned int kbufsz = 65536 ;
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      register int opt = subgetopt_r(argc, argv, "v:b:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'b' : if (!uint0_scan(l.arg, &kbufsz)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
    if (!argc) strerr_dieusage(100, USAGE) ;
    if (!netlink_init_stdin(kbufsz)) strerr_diefu1sys(111, "init netlink") ;
  }

  x[0].fd = selfpipe_init() ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "init selfpipe") ;
  if (sig_ignore(SIGPIPE) < 0) strerr_diefu1sys(111, "ignore SIGPIPE") ;
  {
    sigset_t set ;
    sigemptyset(&set) ;
    sigaddset(&set, SIGTERM) ;
    sigaddset(&set, SIGCHLD) ;
    if (selfpipe_trapset(&set) < 0) strerr_diefu1sys(111, "trap signals") ;
  }

  {
    int fd ;
    pid = child_spawn1_pipe(argv[0], argv, envp, &fd, 0) ;
    if (!pid) strerr_diefu2sys(111, "spawn ", argv[0]) ;
    if (fd_move(1, fd) < 0) strerr_diefu1sys(111, "move pipe to stdout") ;
    if (ndelay_on(1) < 0) strerr_diefu1sys(111, "make stdout nonblocking") ;
  }

  if (verbosity >= 2) strerr_warni1x("starting") ;

  while (cont || buffer_len(buffer_1))
  {
    register int r ;
    x[1].events = buffer_len(&b1) ? IOPAUSE_WRITE : 0 ;
    x[2].events = buffer_available(&b1) >= MAXNLSIZE + 1 ? IOPAUSE_READ : 0 ;
    r = iopause(x, 2 + cont, 0, 0) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    if (!r) continue ;
    if (x[0].revents & IOPAUSE_EXCEPT)
      strerr_diefu1x(111, "iopause: trouble with selfpipe") ;
    if (x[0].revents & IOPAUSE_READ)
      handle_signals() ;
    if (x[1].revents & IOPAUSE_WRITE)
      handle_stdout() ;
    if (cont && x[2].events & IOPAUSE_READ && x[2].revents & IOPAUSE_READ)
      handle_netlink() ;
  }
  if (verbosity >= 2) strerr_warni1x("exiting") ;
  return 0 ;
}

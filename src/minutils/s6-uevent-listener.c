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
#include <asm/types.h>
#include <linux/netlink.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/bytestr.h>
#include <skalibs/uint.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/iopause.h>
#include <skalibs/bufalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/selfpipe.h>

#define USAGE "s6-uevent-listener [ -v verbosity ] [ -b kbufsz ] helperprogram..."
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() strerr_diefu1sys(111, "build string") ;

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
  struct sockaddr_nl nl = { .nl_family = AF_NETLINK, .nl_pad = 0, .nl_groups = 0, .nl_pid = 0 } ;
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

static void doit (char const *s, unsigned int len)
{
  if (!bufalloc_put(bufalloc_1, s, len)) dienomem() ;
}

static void terminate (void)
{
  if (!bufalloc_put(bufalloc_1, "", 1)) dienomem() ;
}

static inline void handle_netlink (void)
{
  char buf[8192] ;
  static int inmulti = 0 ;
  struct sockaddr_nl nl;
  struct iovec iov = { .iov_base = &buf, .iov_len = sizeof(buf) } ;
  struct msghdr msg =
  {
    .msg_name = &nl,
    .msg_namelen = sizeof(struct sockaddr_nl),
    .msg_iov = &iov,
    .msg_iovlen = 1,
    .msg_control = 0,
    .msg_controllen = 0,
    .msg_flags = 0
  } ;
  struct nlmsghdr *nlh = (struct nlmsghdr *)buf ;
  int r = sanitize_read(fd_recvmsg(0, &msg)) ;
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

  for (; NLMSG_OK(nlh, r) ; nlh = NLMSG_NEXT(nlh, r))
    switch (nlh->nlmsg_type)
    {
      case NLMSG_NOOP : break ;
      case NLMSG_ERROR :
        if (verbosity >= 3)
          strerr_warnw2x("spurious NLMSG_ERROR ", "netlink message") ;
        break ;
      case NLMSG_DONE :
        if (inmulti)
        {
          inmulti = 0 ;
          terminate() ;
        }
        else if (verbosity >= 3)
          strerr_warnw2x("spurious NLMSG_DONE ", "netlink message") ;
        break ;
      default :
        if (nlh->nlmsg_flags & NLM_F_MULTI) inmulti = 1 ;
        else if (inmulti)
        {
          inmulti = 0 ;
          terminate() ;
          if (verbosity >= 3)
            strerr_warnw2x("unterminated multipart ", "netlink message") ;
        }
        doit(NLMSG_DATA(nlh), (unsigned int)NLMSG_PAYLOAD(nlh, r)) ;
        if (!inmulti) terminate() ;
        break ;
    }
}

int main (int argc, char const *const *argv, char const *const *envp)
{
  iopause_fd x[3] = { { .events = IOPAUSE_READ }, { .fd = 1 }, { .fd = 0, .events = IOPAUSE_READ } } ;
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
  }

  if (verbosity >= 2) strerr_warni1x("starting") ;

  while (cont || bufalloc_len(bufalloc_1))
  {
    register int r ;
    x[1].events = bufalloc_len(bufalloc_1) ? IOPAUSE_WRITE : 0 ;
    r = iopause(x, 2 + cont, 0, 0) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    if (!r) continue ;
    if (x[0].revents & IOPAUSE_EXCEPT)
      strerr_diefu1x(111, "iopause: trouble with selfpipe") ;
    if (x[0].revents & IOPAUSE_READ) handle_signals() ;
    if (x[1].revents & IOPAUSE_WRITE)
      if (!bufalloc_flush(bufalloc_1)) strerr_diefu1sys(111, "flush stdout") ;
    if (cont && x[2].revents & IOPAUSE_READ) handle_netlink() ;
  }
  if (verbosity >= 2) strerr_warni1x("exiting") ;
  return 0 ;
}

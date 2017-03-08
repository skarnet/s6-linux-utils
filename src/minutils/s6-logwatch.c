/* ISC license. */

#include <sys/types.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/inotify.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/error.h>
#include <skalibs/buffer.h>
#include <skalibs/bufalloc.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>
#include <skalibs/types.h>

#define USAGE "s6-logwatch [ -m maxbuffer ] logdir"
#define dieusage() strerr_dieusage(100, USAGE)

#define N 4096
#define IESIZE 100

typedef enum bstate_e bstate_t, *bstate_t_ref ;
enum bstate_e
{
  B_TAILING = 0,
  B_WAITING = 1
} ;

static void X (void)
{
  strerr_diefu1x(101, "follow file state changes (race condition triggered). Sorry.") ;
}

static size_t nbcat (int fdcurrent)
{
  char buf[N+1] ;
  buffer b = BUFFER_INIT(&fd_readv, fdcurrent, buf, N+1) ;
  struct iovec v[2] ;
  size_t bytes = 0 ;
  for (;;)
  {
    ssize_t r = sanitize_read(buffer_fill(&b)) ;
    if (!r) break ;
    if (r < 0)
    {
      if (errno == EPIPE) break ;
      else strerr_diefu1sys(111, "buffer_fill") ;
    }
    buffer_rpeek(&b, v) ;
    if (!bufalloc_putv(bufalloc_1, v, 2))
      strerr_diefu1sys(111, "bufalloc_putv") ;
    buffer_rseek(&b, r) ;
    bytes += r ;
  }
  return bytes ;
}


int main (int argc, char const *const *argv)
{
  char const *dir = "." ;
  unsigned long maxlen = 4000 ;
  PROG = "s6-logwatch" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "m:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'm' : if (!ulong0_scan(l.arg, &maxlen)) dieusage() ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (argc) dir = *argv ;
  if (chdir(dir) < 0) strerr_diefu2sys(111, "chdir to ", dir) ;
  {
    iopause_fd x[1] = { { -1, IOPAUSE_READ, 0 } } ;
    size_t pos = 0 ;
    int fdcurrent = -1 ;
    int w ;
    bstate_t state = B_TAILING ;
    x[0].fd = inotify_init() ;
    if (x[0].fd < 0) strerr_diefu1sys(111, "inotify_init") ;
    if (ndelay_on(x[0].fd) < 0) strerr_diefu1sys(111, "ndelay_on inotify fd") ;
    w = inotify_add_watch(x[0].fd, ".", IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE) ;
    if (w < 0) strerr_diefu1sys(111, "inotify_add_watch") ;
    if (sig_ignore(SIGPIPE) == -1) strerr_diefu1sys(111, "sig_ignore(SIGPIPE)") ;
    fdcurrent = open_readb("current") ;
    if (fdcurrent < 0)
      if (errno != ENOENT) strerr_diefu1sys(111, "open_readb current") ;
      else state = B_WAITING ;
    else pos = nbcat(fdcurrent) ;

    for (;;)
    {
      int rr ;
      if (!bufalloc_flush(bufalloc_1)) strerr_diefu1sys(111, "write to stdout") ;
      rr = iopause(x, 1, 0, 0) ;
      if (rr < 0) strerr_diefu1sys(111, "iopause") ;
      if (x[0].revents & IOPAUSE_READ)
      {
        char iebuf[IESIZE] ;
        while (bufalloc_len(bufalloc_1) < maxlen)
        {
          size_t i = 0 ;
          ssize_t r = sanitize_read(fd_read(x[0].fd, iebuf, IESIZE)) ;
          if (r < 0) strerr_diefu1sys(111, "read from inotify fd") ;
          if (!r) break ;
          while (i < (size_t)r)
          {
            struct inotify_event *ie = (struct inotify_event *)(iebuf + i) ;
            if ((ie->wd != w) || !ie->len || str_diff(ie->name, "current")) goto cont ;
            if (ie->mask & IN_MODIFY)
            {
              if (state) X() ;
              pos += nbcat(fdcurrent) ;
            }
            else if (ie->mask & IN_CLOSE_WRITE)
            {
              if (state) X() ;
              fd_close(fdcurrent) ;
              fdcurrent = -1 ;
              pos = 0 ;
              state = B_WAITING ;
            }
            else if (ie->mask & IN_CREATE)
            {
              if (!state) X() ;
              fdcurrent = open_readb("current") ;
              if (fdcurrent < 0)
              {
                if (errno != ENOENT) strerr_diefu1sys(111, "open_readb current") ;
                else goto cont ;
              }
              pos = nbcat(fdcurrent) ;
              state = B_TAILING ;
            }
           cont:
            i += sizeof(struct inotify_event) + ie->len ;
          }
        }
      }
    }
  }
  return 0 ;
}

/* ISC license. */

#include <sys/uio.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/inotify.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/sgetopt.h>
#include <skalibs/strerr2.h>
#include <skalibs/error.h>
#include <skalibs/buffer.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>

#define USAGE "s6-logwatch [ logdir ]"
#define dieusage() strerr_dieusage(100, USAGE)

#define B_READING 0
#define B_BLOCKING 1
#define B_WAITING 2
static unsigned int state ;
static int fd ;
static int newcurrent = 0 ;

union inotify_event_u
{
  struct inotify_event event ;
  char buf[sizeof(struct inotify_event) + NAME_MAX + 1] ;
} ;

static void goteof (void)
{
  if (newcurrent)
  {
    fd_close(fd) ;
    fd = open_read("current") ;
    if (fd < 0) strerr_diefu1sys(111, "current") ;
    newcurrent = 0 ;
    state = B_READING ;
  }
  else state = B_BLOCKING ;
}

static int readit (int fd)
{
  struct iovec v[2] ;
  ssize_t r ;
  buffer_wpeek(buffer_1, v) ;
  r = fd_readv(fd, v, 2) ;
  switch (r)
  {
    case -1 : return 0 ;
    case 0 : goteof() ; break ;
    default : buffer_wseek(buffer_1, r) ;
  }
  return 1 ;
}

static void maketransition (unsigned int transition)
{
  static unsigned char const table[3][3] = {
    { 0x10, 0x00, 0x00 },
    { 0x60, 0x22, 0x00 },
    { 0x40, 0x03, 0x02 }
  } ;
  unsigned char c = table[state][transition] ;
  state = c & 0x0f ;
  if (state == 3) strerr_dief1x(101, "current moved twice without being recreated") ;
  if (c & 0x10) newcurrent = 1 ;
  if (c & 0x20) { fd_close(fd) ; fd = -1 ; }
  if (c & 0x40)
  {
    fd = open_read("current") ;
    if (fd < 0) strerr_diefu1sys(111, "current") ;
  }
}

static void handle_event (int ifd, int watch)
{
  ssize_t r ;
  size_t offset = 0 ;
  union inotify_event_u u ;
  r = read(ifd, u.buf, sizeof(u.buf)) ;
  while (r > 0)
  {
    struct inotify_event *event = (struct inotify_event *)(u.buf + offset) ;
    offset += sizeof(struct inotify_event) + event->len ;
    r -= sizeof(struct inotify_event) + event->len ;
    if (event->wd == watch && !strcmp(event->name, "current"))
    {
      int transition = -1 ;
      if (event->mask & IN_CREATE) transition = 0 ;
      else if (event->mask & IN_MOVED_FROM) transition = 1 ;
      else if (event->mask & IN_MODIFY) transition = 2 ;
      if (transition >= 0) maketransition(transition) ;
    }
  }
}

int main (int argc, char const *const *argv)
{
  iopause_fd x[2] = { { .events = IOPAUSE_READ }, { .fd = 1 } } ;
  char const *dir = "." ;
  int watch ;
  unsigned int maxlen = 4096 ;
  PROG = "s6-logwatch" ;
  {
    subgetopt_t l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "m:", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'm' :
          if (!uint0_scan(l.arg, &maxlen)) dieusage() ;
          strerr_warnw1x("the -m option is deprecated") ;
          break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  if (argc) dir = *argv ;
  if (chdir(dir) < 0) strerr_diefu2sys(111, "chdir to ", dir) ;
  if (!fd_sanitize()) strerr_diefu1sys(111, "sanitize standard fds") ;

  x[0].fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC) ;
  if (x[0].fd < 0) strerr_diefu1sys(111, "inotify_init") ;
  watch = inotify_add_watch(x[0].fd, ".", IN_CREATE | IN_MOVED_FROM | IN_MODIFY) ;
  if (watch < 0) strerr_diefu1sys(111, "inotify_add_watch") ;
  fd = open_readb("current") ;
  if (fd < 0)
  {
    if (errno != ENOENT) strerr_diefu3sys(111, "open ", dir, "/current") ;
    state = B_WAITING ;
  }
  else state = B_READING ;
  if (sig_ignore(SIGPIPE) == -1) strerr_diefu1sys(111, "sig_ignore(SIGPIPE)") ;
  if (state == B_READING)
  {
    if (!readit(fd)) strerr_diefu3sys(111, "read from ", dir, "/current") ;
  }

  for (;;)
  {
    int r ;
    x[1].events = buffer_len(buffer_1) ? IOPAUSE_WRITE : 0 ;
    r = iopause(x, 2, 0, 0) ;
    if (r < 0) strerr_diefu1sys(111, "iopause") ;
    if (x[0].revents & IOPAUSE_EXCEPT) x[0].revents |= IOPAUSE_READ ;
    if (x[1].revents & IOPAUSE_EXCEPT) x[1].revents |= IOPAUSE_WRITE ;
    if (x[1].revents & IOPAUSE_WRITE)
    {
      if (!buffer_flush(buffer_1) && !error_isagain(errno))
        strerr_diefu1sys(111, "write to stdout") ;
      if (x[1].revents & IOPAUSE_EXCEPT) break ;
    }
    if (state == B_READING && buffer_available(buffer_1))
    {
      if (!readit(fd)) strerr_diefu3sys(111, "read from ", dir, "/current") ;
    }
    if (x[0].revents & IOPAUSE_READ) handle_event(x[0].fd, watch) ;
  }
  return 0 ;
}

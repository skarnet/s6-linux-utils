/* ISC license. */

#include <skalibs/sysdeps.h>

#ifndef SKALIBS_HASCLOCKBOOT
# error "CLOCK_BOOTTIME required"
#endif

#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <linux/random.h>

#include <skalibs/types.h>
#include <skalibs/sgetopt.h>
#include <skalibs/error.h>
#include <skalibs/strerr.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/iopause.h>
#include <skalibs/blake2s.h>
#include <skalibs/random.h>

#include <s6-linux-utils/config.h>

#define USAGE "rngseed [ -d seeddir ] [ -v verbosity ] [ -r | -R ] [ -n | -N ] [ -w | -W ]"
#define dieusage() strerr_dieusage(100, USAGE)

#define RNGSEED_HASH_PREFIX "SeedRNG v1 Old+New Prefix"

struct rngseed_flags_s
{
  unsigned int read: 1 ;
  unsigned int rcred: 1 ;
  unsigned int block: 1 ;
  unsigned int write: 1 ;
  unsigned int wcred: 1 ;
} ;
#define RNGSEED_FLAGS_ZERO { .read = 0, .rcred = 1, .block = 1, .write = 0, .wcred = 1 }

struct randpoolinfo_s
{
  int entropy_count ;
  int buf_size ;
  char buffer[512]  ;
} ;

static inline void rngseed_mkdirp (char *s, size_t len)
{
  mode_t m = umask(0) ;
  size_t i = 1 ;
  for (; i < len ; i++) if (s[i] == '/')
  {
    s[i] = 0 ;
    if (mkdir(s, 02755) < 0 && errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", s) ;
    s[i] = '/' ;
  }
  umask(m) ;
}

static inline int rngseed_read_seed_nb (char *s, size_t len)
{
  int wcred ;
  size_t w = 0 ;
#ifdef SKALIBS_HASGETRANDOM
  while (w < len)
  {
    ssize_t r = getrandom(s + w, len - w, GRND_NONBLOCK) ;
    if (r == -1)
    {
      if (errno == EINTR) continue ;
      if (error_isagain(errno)) break ;
      strerr_diefu1sys(111, "getrandom") ;
    }
    else w += r ;
  }
  wcred = w >= len ;
  if (!wcred)
#else
  tain dummy = TAIN_EPOCH ;
  iopause_fd x = { .events = IOPAUSE_READ } ;
  x.fd = openbc_read("/dev/random") ;
  if (x.fd == -1) strerr_diefu2sys(111, "open ", "/dev/random") ;
  wcred = iopause(&x, 1, &dummy, &dummy) ;
  if (wcred == -1) strerr_diefu1sys(111, "iopause") ;
  fd_close(x.fd) ;
#endif
  random_devurandom(s + w, len - w) ;
  return wcred ;
}

int main (int argc, char const *const *argv)
{
  blake2s_ctx ctx = BLAKE2S_INIT(32) ;
  char const *seeddir = RNGSEED_DIR ;
  struct rngseed_flags_s flags = RNGSEED_FLAGS_ZERO ;
  unsigned int verbosity = 1 ;
  PROG = "rngseed" ;
  {
    subgetopt l = SUBGETOPT_ZERO ;
    for (;;)
    {
      int opt = subgetopt_r(argc, argv, "d:v:rRnNwW", &l) ;
      if (opt == -1) break ;
      switch (opt)
      {
        case 'd' : seeddir = l.arg ; break ;
        case 'v' : if (!uint0_scan(l.arg, &verbosity)) dieusage() ; break ;
        case 'r' : flags.read = 1 ; flags.rcred = 1 ; break ;
        case 'R' : flags.read = 1 ; flags.rcred = 0 ; break ;
        case 'n' : flags.block = 0 ; break ;
        case 'N' : flags.block = 1 ; break ;
        case 'w' : flags.write = 1 ; flags.wcred = 1 ; break ;
        case 'W' : flags.write = 1 ; flags.wcred = 0 ; break ;
        default : dieusage() ;
      }
    }
    argc -= l.ind ; argv += l.ind ;
  }

  {
    size_t dirlen = strlen(seeddir) ;
    char file[dirlen + 6] ;
    memcpy(file, seeddir, dirlen) ;
    while (dirlen && file[dirlen-1] == '/') dirlen-- ;
    memcpy(file + dirlen, "/seed", 6) ;

    if (flags.write)
    {
      struct timespec ts ;
      if (dirlen)
      {
        file[dirlen] = 0 ;
        rngseed_mkdirp(file, dirlen) ;
        if (mkdir(file, 0700) == -1)
        {
          struct stat st ;
          if (errno != EEXIST) strerr_diefu2sys(111, "mkdir ", file) ;
          if (stat(file, &st) == -1)
            strerr_diefu2sys(111, "stat ", file) ;
          if (st.st_mode & 0077)
          {
            if (verbosity)
              strerr_warnw2sys(file, "has permissive modes, changing it to 0700") ;
            if (chmod(file, 0700) == -1)
              strerr_diefu2sys(111, "chmod ", file) ;
          }
        }
        file[dirlen] = '/' ;
      }
      blake2s_update(&ctx, RNGSEED_HASH_PREFIX, sizeof(RNGSEED_HASH_PREFIX) - 1) ;
      clock_gettime(CLOCK_REALTIME, &ts) ;
      blake2s_update(&ctx, (char *)&ts, sizeof ts) ;
      clock_gettime(CLOCK_BOOTTIME, &ts) ;
      blake2s_update(&ctx, (char *)&ts, sizeof ts) ;
    }

    if (flags.read)
    {
      struct randpoolinfo_s req ;
      struct stat st ;
      size_t seedlen ;
      int fd ;
      if (verbosity >= 2) strerr_warni2x("reading seed from ", file) ;
      fd = openbc_read(file) ;
      if (fd == -1) strerr_diefu2sys(111, "open ", file) ;
      errno = 0 ;
      seedlen = allread(fd, req.buffer, 512) ;
      if (errno) strerr_diefu2sys(111, "read from ", file) ;
      if (!seedlen) strerr_dief2x(100, "empty ", file) ;
      if (fstat(fd, &st) == -1) strerr_diefu2sys(111, "stat ", file) ;
      if (unlink(file) == -1) strerr_diefu2sys(111, "unlink ", file) ;
      fd_close(fd) ;
      if (flags.write)
      {
        blake2s_update(&ctx, (char *)&seedlen, sizeof(seedlen)) ;
        blake2s_update(&ctx, req.buffer, seedlen) ;
      }
      if (flags.rcred && st.st_mode & S_IWUSR && verbosity)
        strerr_warnw2x(file, " was not marked as creditable") ;
      req.entropy_count = flags.rcred && !(st.st_mode & S_IWUSR) ? (seedlen << 3) : 0 ;
      req.buf_size = seedlen ;
      fd = openbc_read("/dev/urandom") ;
      if (fd == -1) strerr_diefu2sys(111, "open ", "/dev/urandom") ;
      if (verbosity >= 2)
      {
        char fmt[SIZE_FMT] ;
        fmt[size_fmt(fmt, seedlen << 3)] = 0 ;
        strerr_warni4x("seeding with ", fmt, " bits", req.entropy_count ? "" : " without crediting") ;
      }
      if (ioctl(fd, RNDADDENTROPY, &req) == -1)
        strerr_diefu1sys(111, "seed") ;
      fd_close(fd) ;
    }

    if (flags.block)
    {
      if (verbosity >= 2)
        strerr_warni1x("waiting for the entropy pool to initialize") ;
#ifdef SKALIBS_HASGETRANDOM
      char c ;
      ssize_t r = getrandom(&c, 1, 0) ;
      if (r == -1) strerr_diefu1sys(111, "getrandom") ;
#else
      iopause_fd x = { .events = IOPAUSE_READ } ;
      x.fd = openbc_read("/dev/random") ;
      if (x.fd == -1) strerr_diefu2sys(111, "open ", "/dev/random") ;
      if (iopause(&x, 1, 0, 0) == -1) strerr_diefu1sys(111, "iopause") ;
      fd_close(x.fd) ;
#endif
    }

    if (flags.write)
    {
      char seed[512] ;
      size_t len = 512 ;
      int wcred = 1 ;
      char s[SIZE_FMT] = "" ;
      int fd = openbc_read("/proc/sys/kernel/random/poolsize") ;
      if (fd < 0)
      {
        if (verbosity) strerr_warnwu2sys("open ", "/proc/sys/kernel/random/poolsize") ;
      }
      else
      {
        size_t r ;
        errno = 0 ;
        r = allread(fd, s, SIZE_FMT - 1) ;
        if (errno)
        {
          if (verbosity) strerr_warnwu2sys("read from ", "/proc/sys/kernel/random/poolsize") ;
        }
        else
        {
          s[r] = 0 ;
          if (!size_scan(s, &r))
          {
            if (verbosity) strerr_warnwu2sys("understand ", "/proc/sys/kernel/random/poolsize") ;
          }
          else len = (r + 7) >> 3 ;
        }
        fd_close(fd) ;
      }
      if (len < 32) len = 32 ;
      if (len > 512) len = 512 ;

      if (verbosity >= 2)
      {
        s[size_fmt(s, len << 3)] = 0 ;
        strerr_warni3x("reading ", s, " bits of random to make the seed") ;
      }
      if (flags.block) random_buf(seed, len) ;
      else wcred = rngseed_read_seed_nb(seed, len) ;
      if (!wcred && verbosity) strerr_warnwu1x("make the seed creditable") ;
      blake2s_update(&ctx, (char *)&len, sizeof(len)) ;
      blake2s_update(&ctx, seed, len) ;
      blake2s_final(&ctx, seed + len - 32) ;
      if (verbosity >= 2) strerr_warni2x("writing seed to ", file) ;
      umask(0077) ;
      if (!openwritenclose_unsafe_sync(file, seed, len))
        strerr_diefu2sys(111, "write to ", file) ;
      if (flags.wcred && wcred)
      {
        if (chmod(file, 0400) == -1 && verbosity)
          strerr_warnwu3sys("mark ", file, "as creditable") ;
      }
    }
  }

  return 0 ;
}

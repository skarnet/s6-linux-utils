/* ISC license. */

#include <skalibs/nonposix.h>

#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/envexec.h>
#include <skalibs/stat.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/skamisc.h>

#include <execline/config.h>

#include <s6-linux-utils/config.h>

#define USAGE "fstab2s6rc [ -A | -a ] [ -U | -u ] [ -E | -e ] [ -F fstab ] [ -m mode ] [ -B bundle ] [ -d basedep ] dir"
#define dieusage() strerr_dieusage(100, USAGE)
#define dienomem() do { strerr_warnfu1sys("stralloc_catb") ; return 111 ; } while (0)

enum fstab_golb_e
{
  FSTAB_GOLB_SKIPNOAUTO = 0x01,
  FSTAB_GOLB_UUID = 0x02,
  FSTAB_GOLB_NOESSENTIAL = 0x04,
} ;

enum fstab_gola_e
{
  FSTAB_GOLA_FSTAB,
  FSTAB_GOLA_MODE,
  FSTAB_GOLA_BUNDLE,
  FSTAB_GOLA_BASEDEP,
  FSTAB_GOLA_N
} ;

enum fstab_flags_e
{
  FSTAB_FLAG_NOFAIL = 0x01,
  FSTAB_FLAG_NOAUTO = 0x02,
  FSTAB_FLAG_ISUUID = 0x04,
  FSTAB_FLAG_ISLABEL = 0x08,
} ;

typedef struct fsent_s fsent, *fsent_ref ;
struct fsent_s
{
  size_t device ;
  size_t mountpoint ;
  size_t qmountpoint ;
  size_t type ;
  size_t opts ;
  size_t servicename ;
  uint32_t flags ;
  genalloc sublist ;  /* fsent */
} ;

typedef struct swapent_s swapent, *swapent_ref ;
struct swapent_s
{
  size_t device ;
  size_t opts ;
  size_t servicename ;
  uint32_t flags ;
} ;

static int process_device (stralloc *sa, char const *dev, uint32_t *flags)
{
  if (!strncmp(dev, "UUID=", 5))
  {
    if (!stralloc_cats(sa, dev + 5) || !stralloc_0(sa)) dienomem() ;
    *flags |= FSTAB_FLAG_ISUUID ;
  }
  else if (!strncmp(dev, "LABEL=", 6))
  {
    if (!string_quotes(sa, dev + 6) || !stralloc_0(sa)) dienomem() ;
    *flags |= FSTAB_FLAG_ISLABEL ;
  }
  else if (!string_quotes(sa, dev) || !stralloc_0(sa)) dienomem() ;
  return 0 ;
}

static int process_opts (stralloc *sa, char *opts, uint32_t *flags)
{
  static char const *const ignore[] =
  {
    "comment",
    "defaults",
    "owner",
    "user",
  } ;
  size_t sabase = sa->len ;
  size_t len = strlen(opts) ;
  size_t pos = 0 ;
  while (pos < len)
  {
    size_t i = str_chr(opts + pos, ',') ;
    opts[pos + i] = 0 ;
    if (!strncmp(opts + pos, "x-", 2)) ;
    else if (!strcmp(opts + pos, "nofail")) *flags |= FSTAB_FLAG_NOFAIL ;
    else if (!strcmp(opts + pos, "noauto")) *flags |= FSTAB_FLAG_NOAUTO ;
    else if (bsearch(opts + pos, ignore, sizeof(ignore)/sizeof(char const *), sizeof(char const *), &str_bcmp)) ;
    else
      if (!stralloc_cats(sa, opts + pos) || !stralloc_catb(sa, ",", 1)) dienomem() ;
    pos += i+1 ;
  }
  if (sa->len > sabase) sa->s[sa->len - 1] = 0 ;
  return 0 ;
}

static int process_servicename (stralloc *sa, char const *name, int isswap)
{
  size_t len = strlen(name) + 1 ;
  size_t i = name[0] == '/' ;
  if (!stralloc_cats(sa, isswap ? "swap-" : "mount-")) dienomem() ;
  while (i < len)
    if (!stralloc_catb(sa, name[i] == '/' ? ":" : name + i, 1)) dienomem() ;
  return 0 ;
}

static inline int add_swap (struct mntent *mnt, genalloc *ga, stralloc *sa, uint64_t options)
{
  swapent f = { .flags = 0 } ;
  int e ;
  f.device = sa->len ;
  if (options & FSTAB_GOLB_UUID)
  {
    e = process_device(sa, mnt->mnt_fsname, &f.flags) ;
    if (e) return e ;
  }
  else
    if (!string_quotes(sa, mnt->mnt_fsname) || !stralloc_0(sa)) dienomem() ;
  f.opts = sa->len ;
  e = process_opts(sa, mnt->mnt_opts, &f.flags) ;
  if (e) return e ;
  f.servicename = sa->len ;
  e = process_servicename(sa, mnt->mnt_fsname, 1) ;
  if (e) return e ;
  if (genalloc_catb(swapent, ga, &f, 1)) dienomem() ;
  return 0 ;
}

static int fstab_insert_mount (fsent *me, genalloc *ga, char const *s)
{
  size_t mylen = strlen(s + me->mountpoint) ;
  fsent *t = genalloc_s(fsent, ga) ;
  unsigned int n = genalloc_len(fsent, ga) ;
  for (unsigned int i = 0 ; i < n ; i++)
  {
    size_t fslen = strlen(s + t[i].mountpoint) ;
    if (mylen == fslen && !memcmp(s + me->mountpoint, s + t[i].mountpoint, mylen))
    {
      strerr_warnf2x("duplicate mount point: ", s + me->mountpoint) ;
      return 1 ;
    }
    if (mylen < fslen && !strncmp(s + t[i].mountpoint, s + me->mountpoint, mylen) && s[t[i].mountpoint + mylen] == '/')
    {
      if (!genalloc_catb(fsent, &me->sublist, t + i, 1)) dienomem() ;
      t[i] = *me ;
      return 0 ;
    }
    if (mylen > fslen && !strncmp(s + me->mountpoint, s + t[i].mountpoint, fslen) && s[me->mountpoint + fslen] == '/')
      return fstab_insert_mount(me, &t[i].sublist, s) ;
  }
  if (!genalloc_catb(fsent, ga, me, 1)) dienomem() ;
  return 0 ;
}

static inline int add_fs (struct mntent *mnt, genalloc *root, stralloc *sa, uint64_t options)
{
  fsent f = { .flags = 0, .sublist = GENALLOC_ZERO } ;
  int e ;
  f.device = sa->len ;
  if (options & FSTAB_GOLB_UUID)
  {
    e = process_device(sa, mnt->mnt_fsname, &f.flags) ;
    if (e) return e ;
  }
  else
    if (!string_quotes(sa, mnt->mnt_fsname) || !stralloc_0(sa)) dienomem() ;
  f.mountpoint = sa->len ;
  if (!stralloc_cats(sa, mnt->mnt_dir + 1) || !stralloc_0(sa)) dienomem() ;
  f.qmountpoint = sa->len ;
  if (!string_quotes(sa, mnt->mnt_dir) || !stralloc_0(sa)) dienomem() ;
  f.type = sa->len ;
  if (!stralloc_cats(sa, mnt->mnt_type) || !stralloc_0(sa)) dienomem() ;
  f.opts = sa->len ;
  e = process_opts(sa, mnt->mnt_opts, &f.flags) ;
  if (e) return e ;
  f.servicename = sa->len ;
  e = process_servicename(sa, mnt->mnt_dir, 0) ;
  if (e) return e ;
  return fstab_insert_mount(&f, root, sa->s) ;
}

static int write_dependencies (char const *dir, char const *myname, fsent const *tab, size_t n, char const *s)
{
  size_t dirlen = strlen(dir) ;
  size_t mylen = myname ? strlen(myname) : 0 ;
  int e ;

  for (size_t i = 0 ; i < n ; i++)
  {
    if (myname)
    {
      size_t slen = strlen(s + tab[i].servicename) ;
      char fn[dirlen + 18 + slen + mylen] ;
      memcpy(fn, dir, dirlen) ;
      fn[dirlen] = '/' ;
      memcpy(fn + dirlen + 1, s + tab[i].servicename, slen) ;
      memcpy(fn + dirlen + 1 + slen, "/dependencies.d/", 16) ;
      memcpy(fn + dirlen + slen + 17, myname, mylen + 1) ;
      if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) { strerr_warnfu2sys("touch ", fn) ; return 111 ; }
    }
    e = write_dependencies(dir, s + tab[i].servicename, genalloc_s(fsent, &tab[i].sublist), genalloc_len(fsent, &tab[i].sublist), s) ;
    if (e) return e ;
  }
  return 0 ;
}

static inline int write_fses (char const *dir, fsent const *tab, size_t n, char const *s, uint64_t options, char const *bundle, char const *basedep)
{
  char buf[4096] ;
  buffer b ;
  int fd ;
  size_t dirlen = strlen(dir) ;
  for (size_t i = 0 ; i < n ; i++)
  {
    mode_t m = umask(0) ;
    size_t slen = strlen(s + tab[i].servicename) ;
    char fn[dirlen + slen + 19] ;
    memcpy(fn, dir, dirlen) ;
    fn[dirlen] = '/' ;
    memcpy(fn + dirlen + 1, s + tab[i].servicename, slen + 1) ;
    if (mkdir(fn, 0755) == -1) { strerr_warnfu2sys("mkdir ", fn) ; return 111 ; }
    memcpy(fn + dirlen + 1 + slen, "/dependencies.d", 16) ;
    if (mkdir(fn, 0755) == -1) { strerr_warnfu2sys("mkdir ", fn) ; return 111 ; }
    umask(m) ;

    memcpy(fn + dirlen + 2 + slen, "type", 5) ;
    if (!openwritenclose_unsafe5(fn, "oneshot\n", 8, 0, 0)) goto err1n ;

    if (!(tab[i].flags & FSTAB_FLAG_NOAUTO))
    {
      memcpy(fn + dirlen + 2 + slen, "flag-", 5) ;
      memcpy(fn + dirlen + 7 + slen, options & FSTAB_GOLB_NOESSENTIAL ? "essential" : "recommended", options & FSTAB_GOLB_NOESSENTIAL ? 10 : 12) ;
      if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) goto err1n ;
    }

    memcpy(fn + dirlen + 2 + slen, "down", 5) ;
    if (tab[i].flags & FSTAB_FLAG_NOAUTO || options & FSTAB_GOLB_NOESSENTIAL)
    {
      fd = open_create(fn) ;
      if (fd == -1) { strerr_warnfu3sys("open ", fn, " for writing") ; return 111 ; }
      buffer_init(&b, &buffer_write, fd, buf, 4096) ;
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "foreground { ") < 0) goto err1 ;
      if (options & FSTAB_GOLB_UUID)
      {
        if (buffer_puts(&b, "umount -l -- ") < 0) goto err1 ;
      }
      else
      {
        if (buffer_puts(&b, S6_LINUX_UTILS_BINPREFIX "s6-umount -- ") < 0) goto err1 ;
      }
      if (buffer_puts(&b, s + tab[i].qmountpoint) < 0
       || buffer_puts(&b, " }\n" EXECLINE_EXTBINPREFIX "exit 0") < 0
       || buffer_putflush(&b, "\n", 1) < 0) goto err1 ;
      fd_close(fd) ;
      goto ok1 ;
     err1:
      fd_close(fd) ;
     err1n:
      strerr_warnfu2sys("write to ", fn) ;
      return 111 ;
     ok1:
    }
    else if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) goto err2n ;

    memcpy(fn + dirlen + 2 + slen, "up", 3) ;
    fd = open_create(fn) ;
    if (fd == -1) { strerr_warnfu3sys("open ", fn, " for writing") ; return 111 ; }
    buffer_init(&b, &buffer_write, fd, buf, 4096) ;
    if (tab[i].flags & FSTAB_FLAG_NOFAIL)
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "foreground { ") < 0) goto err2 ;
    if (options & FSTAB_GOLB_UUID)
    {
      if (buffer_puts(&b, "mount") < 0) goto err2 ;
      if (tab[i].flags & FSTAB_FLAG_ISUUID)
      {
        if (buffer_puts(&b, " -U ") < 0
         || buffer_puts(&b, s + tab[i].device) < 0) goto err2 ;
      }
      else if (tab[i].flags & FSTAB_FLAG_ISLABEL)
      {
        if (buffer_puts(&b, " -L ") < 0
         || buffer_puts(&b, s + tab[i].device) < 0) goto err2 ;
      }
    }
    else
    {
      if (buffer_puts(&b, S6_LINUX_UTILS_BINPREFIX "s6-mount") < 0) goto err2 ;
    }
    if (buffer_puts(&b, " -t ") < 0
     || buffer_puts(&b, s + tab[i].type) < 0) goto err2 ;
    if (s[tab[i].opts])
    {
      if (buffer_puts(&b, " -o ") < 0
       || buffer_puts(&b, s + tab[i].opts) < 0) goto err2 ;
    }
    if (buffer_put(&b, " -- ", 1) < 0) goto err2 ;
    if (!(options & FSTAB_GOLB_UUID && (tab[i].flags & (FSTAB_FLAG_ISUUID | FSTAB_FLAG_ISLABEL))))
    {
      if (buffer_puts(&b, s + tab[i].device) < 0
       || buffer_put(&b, " ", 1) < 0) goto err2 ;
    }
    if (buffer_puts(&b, s + tab[i].qmountpoint) < 0) goto err2 ;
    if (tab[i].flags & FSTAB_FLAG_NOFAIL)
      if (buffer_puts(&b, " }\n" EXECLINE_EXTBINPREFIX "exit 0") < 0) goto err2 ;
    if (buffer_putflush(&b, "\n", 1) < 0) goto err2 ;
    fd_close(fd) ;
    goto ok2 ;
   err2:
    fd_close(fd) ;
   err2n:
    strerr_warnfu2sys("write to ", fn) ;
    return 111 ;
   ok2:
  }

  if (bundle)
  {
    mode_t m = umask(0) ;
    size_t blen = strlen(bundle) ;
    char bdir[dirlen + blen + 20] ;
    memcpy(bdir, dir, dirlen) ;
    memcpy(bdir + dirlen, "-mounts", 7) ;
    bdir[dirlen + 7] = '/' ;
    memcpy(bdir + dirlen + 7, bundle, blen + 1) ;
    if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
    memcpy(bdir + dirlen + blen + 8, "/type", 6) ;
    if (!openwritenclose_unsafe5(bdir, "bundle\n", 7, 0, 0)) { strerr_warnfu2sys("write to ", bdir) ; return 111 ; }
    memcpy(bdir + dirlen + blen + 8, "contents.d", 11) ;
    if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
    umask(m) ;
    bdir[dirlen + blen + 19] = '/' ;
    for (size_t i = 0 ; i < n ; i++) if (!(tab[i].flags & FSTAB_FLAG_NOAUTO))
    {
      size_t slen = strlen(s + tab[i].servicename) ;
      char sfn[dirlen + blen + 21 + slen] ;
      memcpy(sfn, bdir, dirlen + blen + 20) ;
      memcpy(sfn + dirlen + blen + 20, s + tab[i].servicename, slen + 1) ;
      if (!openwritenclose_unsafe5(sfn, "", 0, 0, 0)) { strerr_warnfu2sys("touch ", sfn) ; return 111 ; }
    }
  }

  return write_dependencies(dir, basedep, tab, n, s) ;
}

static inline int write_swaps (char const *dir, swapent const *tab, size_t n, char const *s, uint64_t options, char const *bundle, char const *basedep)
{
  char buf[4096] ;
  buffer b ;
  int fd ;
  size_t dirlen = strlen(dir) ;
  size_t bdlen = basedep ? strlen(basedep) : 0 ;
  for (size_t i = 0 ; i < n ; i++)
  {
    mode_t m = umask(0) ;
    size_t slen = strlen(s + tab[i].servicename) ;
    char fn[dirlen + slen + 19 + bdlen] ;
    memcpy(fn, dir, dirlen) ;
    fn[dirlen] = '/' ;
    memcpy(fn + dirlen + 1, s + tab[i].servicename, slen + 1) ;
    if (mkdir(fn, 0755) == -1) { strerr_warnfu2sys("mkdir ", fn) ; return 111 ; }
    memcpy(fn + dirlen + 1 + slen, "/dependencies.d", 16) ;
    if (mkdir(fn, 0755) == -1) { strerr_warnfu2sys("mkdir ", fn) ; return 111 ; }
    umask(m) ;

    if (basedep)
    {
      fn[dirlen + slen + 16] = '/' ;
      memcpy(fn + dirlen + slen + 17, basedep, bdlen + 1) ;
      if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) goto err1n ;
    }

    memcpy(fn + dirlen + 2 + slen, "type", 5) ;
    if (!openwritenclose_unsafe5(fn, "oneshot\n", 8, 0, 0)) goto err1n ;

    if (!(tab[i].flags & FSTAB_FLAG_NOAUTO))
    {
      memcpy(fn + dirlen + 2 + slen, "flag-", 5) ;
      memcpy(fn + dirlen + 7 + slen, options & FSTAB_GOLB_NOESSENTIAL ? "essential" : "recommended", options & FSTAB_GOLB_NOESSENTIAL ? 10 : 12) ;
      if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) goto err1n ;
    }

    memcpy(fn + dirlen + 2 + slen, "down", 5) ;
    if (tab[i].flags & FSTAB_FLAG_NOAUTO || options & FSTAB_GOLB_NOESSENTIAL)
    {
      fd = open_create(fn) ;
      if (fd == -1) { strerr_warnfu3sys("open ", fn, " for writing") ; return 111 ; }
      buffer_init(&b, &buffer_write, fd, buf, 4096) ;
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "foreground { ") < 0) goto err1 ;
      if (options & FSTAB_GOLB_UUID)
      {
        if (buffer_puts(&b, "swapoff ") < 0) goto err1 ;
        if (tab[i].flags & FSTAB_FLAG_ISUUID)
        {
          if (buffer_puts(&b, "-U ") < 0) goto err1 ;
        }
        else if (tab[i].flags & FSTAB_FLAG_ISLABEL)
        {
          if (buffer_puts(&b, "-L ") < 0) goto err1 ;
        }
      }
      else
      {
        if (buffer_puts(&b, S6_LINUX_UTILS_BINPREFIX "s6-swapoff -- ") < 0) goto err1 ;
      }
      if (buffer_puts(&b, s + tab[i].device) < 0
       || buffer_puts(&b, " }\n" EXECLINE_EXTBINPREFIX "exit 0") < 0
       || buffer_putflush(&b, "\n", 1) < 0) goto err1 ;
      fd_close(fd) ;
      goto ok1 ;
     err1:
      fd_close(fd) ;
     err1n:
      strerr_warnfu2sys("write to ", fn) ;
      return 111 ;
     ok1:
    }
    else if (!openwritenclose_unsafe5(fn, "", 0, 0, 0)) goto err2n ;

    memcpy(fn + dirlen + 2 + slen, "up", 3) ;
    fd = open_create(fn) ;
    if (fd == -1) { strerr_warnfu3sys("open ", fn, " for writing") ; return 111 ; }
    buffer_init(&b, &buffer_write, fd, buf, 4096) ;
    if (tab[i].flags & FSTAB_FLAG_NOFAIL)
      if (buffer_puts(&b, EXECLINE_EXTBINPREFIX "foreground { ") < 0) goto err2 ;
    if (options & FSTAB_GOLB_UUID)
    {
      if (buffer_puts(&b, "swapon") < 0) goto err2 ;
      if (tab[i].flags & FSTAB_FLAG_ISUUID)
      {
        if (buffer_puts(&b, " -U ") < 0
         || buffer_puts(&b, s + tab[i].device) < 0) goto err2 ;
      }
      else if (tab[i].flags & FSTAB_FLAG_ISLABEL)
      {
        if (buffer_puts(&b, " -L ") < 0
         || buffer_puts(&b, s + tab[i].device) < 0) goto err2 ;
      }
      if (s[tab[i].opts])
      {
        if (buffer_puts(&b, " -o ") < 0
         || buffer_puts(&b, s + tab[i].opts) < 0) goto err2 ;
      }
    }
    else
    {
      if (buffer_puts(&b, S6_LINUX_UTILS_BINPREFIX "s6-swapon --") < 0) goto err2 ;
    }
    if (buffer_put(&b, " ", 1) < 0
     || buffer_puts(&b, s + tab[i].device) < 0) goto err2 ;
    if (tab[i].flags & FSTAB_FLAG_NOFAIL)
      if (buffer_puts(&b, " }\n" EXECLINE_EXTBINPREFIX "exit 0") < 0) goto err2 ;
    if (buffer_putflush(&b, "\n", 1) < 0) goto err2 ;
    fd_close(fd) ;
    goto ok2 ;
   err2:
    fd_close(fd) ;
   err2n:
    strerr_warnfu2sys("write to ", fn) ;
    return 111 ;
   ok2:
  }

  if (bundle)
  {
    size_t blen = strlen(bundle) ;
    char bdir[dirlen + blen + 19] ;
    memcpy(bdir, dir, dirlen) ;
    memcpy(bdir + dirlen, "-swaps", 6) ;
    bdir[dirlen + 6] = '/' ;
    memcpy(bdir + dirlen + 7, bundle, blen + 1) ;
    if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
    memcpy(bdir + dirlen + blen + 7, "/type", 6) ;
    if (!openwritenclose_unsafe5(bdir, "bundle\n", 7, 0, 0)) { strerr_warnfu2sys("write to ", bdir) ; return 111 ; }
    memcpy(bdir + dirlen + blen + 7, "contents.d", 11) ;
    if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
    bdir[dirlen + blen + 18] = '/' ;
    for (size_t i = 0 ; i < n ; i++) if (!(tab[i].flags & FSTAB_FLAG_NOAUTO))
    {
      size_t slen = strlen(s + tab[i].servicename) ;
      char sfn[dirlen + blen + 20 + slen] ;
      memcpy(sfn, bdir, dirlen + blen + 19) ;
      memcpy(sfn + dirlen + blen + 19, s + tab[i].servicename, slen + 1) ;
      if (!openwritenclose_unsafe5(sfn, "", 0, 0, 0)) { strerr_warnfu2sys("touch ", sfn) ; return 111 ; }
    }
  }
  return 0 ;
}

static inline int write_bundle (char const *dir, char const *bundle)
{
  mode_t m = umask(0) ;
  size_t dirlen = strlen(dir) ;
  size_t blen = strlen(bundle) ;
  char bdir[dirlen + 22 + 2 * blen] ;
  memcpy(bdir, dir, dirlen) ;
  bdir[dirlen] = '/' ;
  memcpy(bdir + dirlen + 1, bundle, blen + 1) ;
  if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
  memcpy(bdir + dirlen + blen + 1, "/type", 6) ;
  if (!openwritenclose_unsafe5(bdir, "bundle\n", 7, 0, 0)) { strerr_warnfu2sys("write to ", bdir) ; return 111 ; }
  memcpy(bdir + dirlen + blen + 2, "contents.d", 11) ;
  if (mkdir(bdir, 0755) == -1) { strerr_warnfu2sys("mkdir ", bdir) ; return 111 ; }
  umask(m) ;
  bdir[dirlen + blen + 13] = '/' ;
  memcpy(bdir + dirlen + blen + 14, bundle, blen) ;
  memcpy(bdir + dirlen + 14 + 2 * blen, "-swaps", 7) ;
  if (!openwritenclose_unsafe5(bdir, "", 0, 0, 0)) { strerr_warnfu2sys("touch ", bdir) ; return 111 ; }
  memcpy(bdir + dirlen + 15 + 2 * blen, "mounts", 7) ;
  if (!openwritenclose_unsafe5(bdir, "", 0, 0, 0)) { strerr_warnfu2sys("touch ", bdir) ; return 111 ; }
  return 0 ;
}

static inline int doit (char const *fstab, char const *dir, uint64_t options, char const *bundle, char const *basedep, stralloc *sa)
{
  genalloc rootfs = GENALLOC_ZERO ;  /* fsent */
  genalloc swaps = GENALLOC_ZERO ;  /* swapent */
  int e ;
  FILE *in = setmntent(fstab, "re") ;
  if (!in) { strerr_warnfu2sys("open ", fstab) ; return 111 ; }
  for (;;)
  {
    struct mntent *mnt ;
    errno = 0 ;
    mnt = getmntent(in) ;
    if (!mnt) break ;
    if (mnt->mnt_dir[0] != '/') continue ;  /* "none" or "swap" or ... */
    if (!mnt->mnt_dir[1]) continue ;  /* skip rootfs */
    if (options & FSTAB_GOLB_SKIPNOAUTO && hasmntopt(mnt, "noauto")) continue ;
    if (!strcmp(mnt->mnt_type, "swap")) e = add_swap(mnt, &swaps, sa, options) ;
    else e = add_fs(mnt, &rootfs, sa, options) ;
    if (e)
    {
      endmntent(in) ;
      return e ;
    }
  }
  if (errno)
  {
    strerr_warnfu2sys("getmntent ", fstab) ;
    endmntent(in) ;
    return 111 ;
  }
  endmntent(in) ;

  e = write_fses(dir, genalloc_s(fsent, &rootfs), genalloc_len(fsent, &rootfs), sa->s, options, bundle, basedep) ;
  if (e) return e ;
  e = write_swaps(dir, genalloc_s(swapent, &swaps), genalloc_len(swapent, &swaps), sa->s, options, bundle, basedep) ;
  if (e) return e ;
  if (bundle)
  {
    e = write_bundle(dir, bundle) ;
    if (e) return e ;
  }
  return 0 ;
}

int main (int argc, char const *const *argv)
{
  static gol_bool const rgolb[] =
  {
    { .so = 'A', .lo = "include-noauto", .clear = FSTAB_GOLB_SKIPNOAUTO, .set = 0 },
    { .so = 'a', .lo = "exclude-noauto", .clear = 0, .set = FSTAB_GOLB_SKIPNOAUTO },
    { .so = 'U', .lo = "without-uuid", .clear = FSTAB_GOLB_UUID, .set = 0 },
    { .so = 'u', .lo = "with-uuid", .clear = 0, .set = FSTAB_GOLB_UUID },
    { .so = 'E', .lo = "essential", .clear = FSTAB_GOLB_NOESSENTIAL, .set = 0 },
    { .so = 'e', .lo = "not-essential", .clear = 0, .set = FSTAB_GOLB_NOESSENTIAL },
  } ;
  static gol_arg const rgola[] =
  {
    { .so = 'F', .lo = "fstab", .i = FSTAB_GOLA_FSTAB },
    { .so = 'm', .lo = "mode", .i = FSTAB_GOLA_MODE },
    { .so = 'B', .lo = "bundle", .i = FSTAB_GOLA_BUNDLE },
    { .so = 'd', .lo = "base-dependency", .i = FSTAB_GOLA_BASEDEP },
  } ;
  stralloc storage = STRALLOC_ZERO ;
  uint64_t wgolb = 0 ;
  unsigned int mode = 0755 ;
  unsigned int golc ;
  char const *wgola[FSTAB_GOLA_N] = { 0 } ;

  PROG = "fstab2s6rc" ;
  wgola[FSTAB_GOLA_FSTAB] = "/etc/fstab" ;
  golc = GOL_main(argc, argv, rgolb, rgola, &wgolb, wgola) ;
  argc -= golc ; argv += golc ;
  if (!argc) dieusage() ;
  if (wgola[FSTAB_GOLA_MODE] && !uint0_oscan(wgola[FSTAB_GOLA_MODE], &mode)) dieusage() ;

  if (access(argv[0], F_OK) == -1)
  {
    if (errno != ENOENT) strerr_diefu2sys(111, "access ", argv[0]) ;
  }
  else strerr_dief2x(111, argv[0], "already exists") ;

  {
    mode_t m = umask(0) ;
    int e ;
    size_t dirlen = strlen(argv[0]) ;
    char dir[dirlen + 8] ;
    memcpy(dir, argv[0], dirlen) ;
    memcpy(dir + dirlen, ":XXXXXX", 8) ;
    if (!mkdtemp(dir)) strerr_diefu2sys(111, "mkdtemp ", dir) ;
    umask(m) ;
    e = doit(wgola[FSTAB_GOLA_FSTAB], dir, wgolb, wgola[FSTAB_GOLA_BUNDLE], wgola[FSTAB_GOLA_BASEDEP], &storage) ;
    if (e)
    {
      rm_rf(dir) ;
      _exit(e) ;
    }
    if (chmod(dir, mode) == -1)
    {
      rm_rf(dir) ;
      strerr_diefu2sys(111, "chmod ", dir) ;
    }
    if (rename(dir, argv[0]) == -1)
    {
      rm_rf(dir) ;
      strerr_diefu4sys(111, "rename ", dir, " to ", argv[0]) ;
    }
  }
  return 0 ;
}

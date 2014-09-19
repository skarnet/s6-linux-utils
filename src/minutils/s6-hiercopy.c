/* ISC license. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <skalibs/bytestr.h>
#include <skalibs/strerr2.h>
#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/direntry.h>
#include <skalibs/skamisc.h>

#define USAGE "s6-hiercopy src dst"

static void hiercopy (char const *, char const *) ;

static int filecopy (char const *src, char const *dst, mode_t mode)
{
  int d ;
  int s = open_readb(src) ;
  if (s < 0) return 0 ;
  d = open3(dst, O_WRONLY | O_CREAT | O_TRUNC, mode) ;
  if (d < 0)
  {
    fd_close(s) ;
    return 0 ;
  }
  if (fd_cat(s, d) < 0) goto err ;
  fd_close(s) ;
  fd_close(d) ;
  return 1 ;

err:
  {
    register int e = errno ;
    fd_close(s) ;
    fd_close(d) ;
    errno = e ;
  }
  return 0 ;
}

static int dircopy (char const *src, char const *dst, mode_t mode)
{
  unsigned int tmpbase = satmp.len ;
  unsigned int maxlen = 0 ;
  {
    DIR *dir = opendir(src) ;
    if (!dir) return 0 ;
    for (;;)
    {
      direntry *d ;
      register unsigned int n ;
      errno = 0 ;
      d = readdir(dir) ;
      if (!d) break ;
      if (d->d_name[0] == '.')
        if (((d->d_name[1] == '.') && !d->d_name[2]) || !d->d_name[1])
          continue ;
      n = str_len(d->d_name) ;
      if (n > maxlen) maxlen = n ;
      if (!stralloc_catb(&satmp, d->d_name, n+1)) break ;
    }
    if (errno)
    {
      int e = errno ;
      dir_close(dir) ;
      errno = e ;
      goto err ;
    }
    dir_close(dir) ;
  }

  if (mkdir(dst, S_IRWXU) == -1)
  {
    struct stat st ;
    if (errno != EEXIST) goto err ;
    if (stat(dst, &st) < 0) goto err ;
    if (!S_ISDIR(st.st_mode)) { errno = ENOTDIR ; goto err ; }
  }

  {
    unsigned int srclen = str_len(src) ;
    unsigned int dstlen = str_len(dst) ;
    unsigned int i = tmpbase ;
    char srcbuf[srclen + maxlen + 2] ;
    char dstbuf[dstlen + maxlen + 2] ;
    byte_copy(srcbuf, srclen, src) ;
    byte_copy(dstbuf, dstlen, dst) ;
    srcbuf[srclen] = '/' ;
    dstbuf[dstlen] = '/' ;
    while (i < satmp.len)
    {
      register unsigned int n = str_len(satmp.s + i) + 1 ;
      byte_copy(srcbuf + srclen + 1, n, satmp.s + i) ;
      byte_copy(dstbuf + dstlen + 1, n, satmp.s + i) ;
      i += n ;
      hiercopy(srcbuf, dstbuf) ;
    }
  }
  if (chmod(dst, mode) == -1) goto err ;
  satmp.len = tmpbase ;
  return 1 ;
err:
  satmp.len = tmpbase ;
  return 0 ;
}

static void hiercopy (char const *src, char const *dst)
{
  struct stat st ;
  if (lstat(src, &st) == -1) strerr_diefu2sys(111, "stat ", src) ;
  if (S_ISREG(st.st_mode))
  {
    if (!filecopy(src, dst, st.st_mode))
      strerr_diefu4sys(111, "copy regular file ", src, " to ", dst) ;
  }
  else if (S_ISDIR(st.st_mode))
  {
    if (!dircopy(src, dst, st.st_mode))
      strerr_diefu4sys(111, "recursively copy directory ", src, " to ", dst) ;
  }
  else if (S_ISFIFO(st.st_mode))
  {
    if (mkfifo(dst, st.st_mode) < 0)
      strerr_diefu2sys(111, "mkfifo ", dst) ;
  }
  else if (S_ISLNK(st.st_mode))
  {
    unsigned int tmpbase = satmp.len ;
    if ((sareadlink(&satmp, src) < 0) || !stralloc_0(&satmp))
      strerr_diefu2sys(111, "readlink ", src) ;
    if (symlink(satmp.s + tmpbase, dst) < 0)
      strerr_diefu4sys(111, "symlink ", satmp.s + tmpbase, " to ", dst) ;
    satmp.len = tmpbase ;
  }
  else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISSOCK(st.st_mode))
  {
    if (mknod(dst, st.st_mode, st.st_rdev) < 0)
      strerr_diefu2sys(111, "mknod ", dst) ;
  }
  else strerr_dief2x(111, "unrecognized file type for ", src) ;
  lchown(dst, st.st_uid, st.st_gid) ;
  if (!S_ISLNK(st.st_mode)) chmod(dst, st.st_mode) ;
}

int main (int argc, char const *const *argv)
{
  PROG = "s6-hiercopy" ;
  if (argc < 3) strerr_dieusage(100, USAGE) ;
  umask(0) ;
  hiercopy(argv[1], argv[2]) ;
  return 0 ;
}

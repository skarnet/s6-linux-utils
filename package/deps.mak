#
# This file has been generated by tools/gen-deps.sh
#

src/minutils/rngseed.o src/minutils/rngseed.lo: src/minutils/rngseed.c src/include/s6-linux-utils/config.h
src/minutils/s6-chroot.o src/minutils/s6-chroot.lo: src/minutils/s6-chroot.c
src/minutils/s6-freeramdisk.o src/minutils/s6-freeramdisk.lo: src/minutils/s6-freeramdisk.c
src/minutils/s6-hostname.o src/minutils/s6-hostname.lo: src/minutils/s6-hostname.c
src/minutils/s6-logwatch.o src/minutils/s6-logwatch.lo: src/minutils/s6-logwatch.c
src/minutils/s6-mount.o src/minutils/s6-mount.lo: src/minutils/s6-mount.c src/minutils/mount-constants.h
src/minutils/s6-pivotchroot.o src/minutils/s6-pivotchroot.lo: src/minutils/s6-pivotchroot.c
src/minutils/s6-ps.o src/minutils/s6-ps.lo: src/minutils/s6-ps.c src/minutils/s6-ps.h
src/minutils/s6-swapoff.o src/minutils/s6-swapoff.lo: src/minutils/s6-swapoff.c
src/minutils/s6-swapon.o src/minutils/s6-swapon.lo: src/minutils/s6-swapon.c
src/minutils/s6-umount.o src/minutils/s6-umount.lo: src/minutils/s6-umount.c
src/minutils/s6ps_grcache.o src/minutils/s6ps_grcache.lo: src/minutils/s6ps_grcache.c src/minutils/s6-ps.h
src/minutils/s6ps_otree.o src/minutils/s6ps_otree.lo: src/minutils/s6ps_otree.c src/minutils/s6-ps.h
src/minutils/s6ps_pfield.o src/minutils/s6ps_pfield.lo: src/minutils/s6ps_pfield.c src/minutils/s6-ps.h
src/minutils/s6ps_pwcache.o src/minutils/s6ps_pwcache.lo: src/minutils/s6ps_pwcache.c src/minutils/s6-ps.h
src/minutils/s6ps_statparse.o src/minutils/s6ps_statparse.lo: src/minutils/s6ps_statparse.c src/minutils/s6-ps.h
src/minutils/s6ps_ttycache.o src/minutils/s6ps_ttycache.lo: src/minutils/s6ps_ttycache.c src/minutils/s6-ps.h
src/minutils/s6ps_wchan.o src/minutils/s6ps_wchan.lo: src/minutils/s6ps_wchan.c src/minutils/s6-ps.h

rngseed: EXTRA_LIBS := -lskarnet ${SYSCLOCK_LIB}
rngseed: src/minutils/rngseed.o
s6-chroot: EXTRA_LIBS := -lskarnet
s6-chroot: src/minutils/s6-chroot.o
s6-freeramdisk: EXTRA_LIBS := -lskarnet
s6-freeramdisk: src/minutils/s6-freeramdisk.o
s6-hostname: EXTRA_LIBS := -lskarnet
s6-hostname: src/minutils/s6-hostname.o
s6-logwatch: EXTRA_LIBS := -lskarnet
s6-logwatch: src/minutils/s6-logwatch.o
s6-mount: EXTRA_LIBS := -lskarnet
s6-mount: src/minutils/s6-mount.o
s6-pivotchroot: EXTRA_LIBS := -lskarnet
s6-pivotchroot: src/minutils/s6-pivotchroot.o
s6-ps: EXTRA_LIBS := -lskarnet ${MAYBEPTHREAD_LIB}
s6-ps: src/minutils/s6-ps.o src/minutils/s6ps_statparse.o src/minutils/s6ps_otree.o src/minutils/s6ps_pfield.o src/minutils/s6ps_pwcache.o src/minutils/s6ps_grcache.o src/minutils/s6ps_ttycache.o src/minutils/s6ps_wchan.o ${LIBNSSS}
s6-swapoff: EXTRA_LIBS := -lskarnet
s6-swapoff: src/minutils/s6-swapoff.o
s6-swapon: EXTRA_LIBS := -lskarnet
s6-swapon: src/minutils/s6-swapon.o
s6-umount: EXTRA_LIBS := -lskarnet
s6-umount: src/minutils/s6-umount.o

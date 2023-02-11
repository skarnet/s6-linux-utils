/* ISC license. */

#ifndef MOUNT_CONSTANTS_H
#define MOUNT_CONSTANTS_H

/* taken from util-linux */

#ifndef MS_RDONLY
#define MS_RDONLY	 1	/* Mount read-only */
#endif
#ifndef MS_NOSUID
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#endif
#ifndef MS_NODEV
#define MS_NODEV	 4	/* Disallow access to device special files */
#endif
#ifndef MS_NOEXEC
#define MS_NOEXEC	 8	/* Disallow program execution */
#endif
#ifndef MS_SYNCHRONOUS
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#endif
#ifndef MS_REMOUNT
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#endif
#ifndef MS_MANDLOCK
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#endif
#ifndef MS_DIRSYNC
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#endif
#ifndef MS_NOSYMFOLLOW
#define MS_NOSYMFOLLOW	256	/* Don't follow symlinks */
#endif
#ifndef MS_NOATIME
#define MS_NOATIME	0x400	/* 1024: Do not update access times. */
#endif
#ifndef MS_NODIRATIME
#define MS_NODIRATIME   0x800	/* 2048: Don't update directory access times */
#endif
#ifndef MS_BIND
#define	MS_BIND		0x1000	/* 4096: Mount existing tree also elsewhere */
#endif
#ifndef MS_MOVE
#define MS_MOVE		0x2000	/* 8192: Atomically move tree */
#endif
#ifndef MS_REC
#define MS_REC		0x4000	/* 16384: Recursive loopback */
#endif
#ifndef MS_SILENT
#define MS_SILENT	0x8000	/* 32768: Don't emit certain kernel messages */
#endif
#ifndef MS_UNBINDABLE
#define MS_UNBINDABLE	(1<<17)	/* 131072: Make unbindable */
#endif
#ifndef MS_PRIVATE
#define MS_PRIVATE	(1<<18)	/* 262144: Make private */
#endif
#ifndef MS_SLAVE
#define MS_SLAVE	(1<<19)	/* 524288: Make slave */
#endif
#ifndef MS_SHARED
#define MS_SHARED	(1<<20)	/* 1048576: Make shared */
#endif
#ifndef MS_RELATIME
#define MS_RELATIME	(1<<21) /* 2097152: Update atime relative to mtime/ctime */
#endif
#ifndef MS_I_VERSION
#define MS_I_VERSION	(1<<23)	/* Update the inode I_version field */
#endif
#ifndef MS_STRICTATIME
#define MS_STRICTATIME	(1<<24) /* Always perform atime updates */
#endif
#ifndef MS_LAZYTIME
#define MS_LAZYTIME     (1<<25) /* Update the on-disk [acm]times lazily */
#endif

#endif

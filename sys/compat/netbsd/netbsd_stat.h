/*	$OpenBSD: netbsd_stat.h,v 1.6 2005/11/09 14:14:06 martin Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stat.h	8.12 (Berkeley) 8/17/94
 */

/*
 * On systems with 8 byte longs and 4 byte time_ts, padding the time_ts
 * is required in order to have a consistent ABI.  This is because the
 * stat structure used to contain timespecs, which had different
 * alignment constraints than a time_t and a long alone.  The padding
 * should be removed the next time the stat structure ABI is changed.
 * (This will happen whenever we change to 8 byte time_t.)
 */
#if defined(__alpha__)			/* XXX XXX XXX */
#define	__STATPAD(x)	int x;
#else
#define	__STATPAD(x)	/* nothing */
#endif

struct netbsd_stat {
	dev_t	  st_dev;		/* inode's device */
	ino_t	  st_ino;		/* inode's number */
	netbsd_mode_t	  st_mode;	/* inode protection mode */
	netbsd_nlink_t	  st_nlink;	/* number of hard links */
	uid_t	  st_uid;		/* user ID of the file's owner */
	gid_t	  st_gid;		/* group ID of the file's group */
	dev_t	  st_rdev;		/* device type */
#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
	struct	  timespec st_atimespec;/* time of last access */
	struct	  timespec st_mtimespec;/* time of last data modification */
	struct	  timespec st_ctimespec;/* time of last file status change */
#else
	__STATPAD(__pad0)
	time_t	  st_atime;		/* time of last access */
	__STATPAD(__pad1)
	long	  st_atimensec;		/* nsec of last access */
	time_t	  st_mtime;		/* time of last data modification */
	__STATPAD(__pad2)
	long	  st_mtimensec;		/* nsec of last data modification */
	time_t	  st_ctime;		/* time of last file status change */
	__STATPAD(__pad3)
	long	  st_ctimensec;		/* nsec of last file status change */
#endif
	off_t	  st_size;		/* file size, in bytes */
	netbsd_blkcnt_t  st_blocks;	/* blocks allocated for file */
	netbsd_blksize_t st_blksize;	/* optimal blocksize for I/O */
	u_int32_t st_flags;		/* user defined flags for file */
	u_int32_t st_gen;		/* file generation number */
	int64_t	  st_qspare[2];
};

#undef __STATPAD

#if !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE)
#define	st_atime	st_atimespec.tv_sec
#define	st_atimensec	st_atimespec.tv_nsec
#define	st_mtime	st_mtimespec.tv_sec
#define	st_mtimensec	st_mtimespec.tv_nsec
#define	st_ctime	st_ctimespec.tv_sec
#define	st_ctimensec	st_ctimespec.tv_nsec
#endif

/*	$OpenBSD: filesys.h,v 1.2 2003/06/03 02:56:14 millert Exp $	*/

/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
 */

/*
 * $From: filesys.h,v 1.2 1999/08/04 15:57:31 christos Exp $
 * @(#)filesys.h
 */

#ifndef __filesys_h__
#define __filesys_h__

/*
 * File System information
 */

/*
 * Mount information
 */
#if FSI_TYPE == FSI_GETMNT
#	include <sys/types.h>
#	include <sys/param.h>
#	include <sys/mount.h>
#	define MOUNTED_FILE		"<none>"
#endif

#if FSI_TYPE == FSI_GETFSSTAT
#	include <sys/types.h>
#	include <sys/mount.h>
#	define MOUNTED_FILE		"<none>"
#endif

#if FSI_TYPE == FSI_MNTCTL
#	include <sys/mntctl.h>
#	define MOUNTED_FILE		"<none>"
#endif

#if FSI_TYPE == FSI_GETMNTENT
#	include <mntent.h>
#	define	MOUNTED_FILE		MOUNTED
#endif

#if FSI_TYPE == FSI_GETMNTENT2
#if     defined(MNTTAB_H)
#       include MNTTAB_H
#endif	/* MNTTAB_H */
#if     defined(MNTENT_H)
#       include MNTENT_H
#endif	/* MNTENT_H */
#	define	MOUNTED_FILE		MNTTAB
#endif	/* FSI_GETMNTENT2 */

#if	!defined(MOUNTED_FILE) && defined(MNT_MNTTAB)	/* HPUX */
#	define MOUNTED_FILE		MNT_MNTTAB
#endif	/* MNT_MNTTAB */

/*
 * NCR OS defines bcopy and bzero
 */
#if defined(NCR)
#undef bcopy
#undef bzero
#endif	/* NCR */

/*
 * Stat Filesystem
 */
#if 	defined(STATFS_TYPE)
#  if defined(ultrix)
	typedef struct fs_data		statfs_t;
#	define f_bavail			fd_req.bfreen
#	define f_bsize			fd_req.bsize
#	define f_ffree			fd_req.gfree
#  elif defined(_AIX) || STATFS_TYPE == STATFS_SYSV
#	include <sys/statfs.h>
	typedef struct statfs		statfs_t;
#	define f_bavail			f_bfree
#  elif defined(SVR4)
#	include <sys/statvfs.h>
	typedef struct statvfs		statfs_t;
#	define statfs(mp,sb)		statvfs(mp,sb)
#  elif STATFS_TYPE == STATFS_44BSD || STATFS_TYPE == STATFS_OSF1
	typedef struct statfs		statfs_t;
#  else
#	include <sys/vfs.h>
	typedef struct statfs 		statfs_t;
#  endif
#endif	/* STATFS_TYPE */

/*
 * Mount Entry definetions
 */
#ifndef METYPE_OTHER
#define METYPE_OTHER			"other"
#endif
#ifndef METYPE_NFS
#define METYPE_NFS			"nfs"
#endif
#ifndef MEFLAG_READONLY
#define MEFLAG_READONLY			0x01
#endif
#ifndef MEFLAG_IGNORE
#define MEFLAG_IGNORE			0x02
#endif

/*
 * Our internal mount entry type
 */
struct _mntent {
	char			       *me_path;	/* Mounted path */
	char			       *me_type;	/* Type of mount */
	int				me_flags;	/* Mount flags */
};
typedef struct _mntent mntent_t;

/*
 * Internal mount information type
 */
struct mntinfo {
	mntent_t			*mi_mnt;
	struct stat			*mi_statb;
	struct mntinfo			*mi_nxt;
};

/*
 * Declarations
 */
FILE	       *setmountent(const char *, const char *);
mntent_t       *getmountent(FILE *);
mntent_t       *newmountent(const mntent_t *);
void		endmountent(FILE *);

#endif	/* __filesys_h__ */

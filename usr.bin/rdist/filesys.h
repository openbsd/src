/*	$OpenBSD: filesys.h,v 1.4 2014/07/05 10:21:24 guenther Exp $	*/

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
int	        setmountent(void);
mntent_t       *getmountent(void);
mntent_t       *newmountent(const mntent_t *);
void		endmountent(void);

#endif	/* __filesys_h__ */

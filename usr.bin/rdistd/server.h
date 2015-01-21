/*	$OpenBSD: server.h,v 1.2 2015/01/21 04:08:37 guenther Exp $	*/

#ifndef __SERVER_H__
#define __SERVER_H__
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
 * $From: defs.h,v 1.6 2001/03/12 18:16:30 kim Exp $
 * @(#)defs.h      5.2 (Berkeley) 3/20/86
 */

#include <sys/stat.h>

#include "defs.h"

/*
 * Suffix to use when saving files
 */
#ifndef SAVE_SUFFIX
#define SAVE_SUFFIX	".OLD"
#endif


#define MEFLAG_READONLY			0x01
#define MEFLAG_IGNORE			0x02
#define MEFLAG_NFS			0x04

/*
 * Our internal mount entry type
 */
typedef struct {
	char			       *me_path;	/* Mounted path */
	int				me_flags;	/* Mount flags */
} mntent_t;

/*
 * Internal mount information type
 */
struct mntinfo {
	mntent_t			*mi_mnt;
	dev_t				mi_dev;
	struct mntinfo			*mi_nxt;
};

/* filesys-os.c */
int	        setmountent(void);
mntent_t       *getmountent(void);
mntent_t       *newmountent(const mntent_t *);
void		endmountent(void);

/* filesys.c */
char		*find_file(char *, struct stat *, int *);
mntent_t	*findmnt(struct stat *, struct mntinfo *);
int		isdupmnt(mntent_t *, struct mntinfo *);
void		wakeup(int);
struct mntinfo	*makemntinfo(struct mntinfo *);
mntent_t	*getmntpt(char *, struct stat *, int *);
int		is_nfs_mounted(char *, struct stat *, int *);
int		is_ro_mounted(char *, struct stat *, int *);
int		is_symlinked(char *, struct stat *, int *);
int		getfilesysinfo(char *, int64_t *, int64_t *);

/* server.c */
void		server(void);

#endif	/* __SERVER_H__ */

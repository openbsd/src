/*	$OpenBSD: file.h,v 1.44 2007/06/01 17:47:47 niallo Exp $	*/
/*
 * Copyright (c) 2006 Joris Vink <joris@openbsd.org>
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FILE_H
#define FILE_H

#include <sys/queue.h>

#include <dirent.h>
#include <stdio.h>

#include "rcs.h"

struct cvs_file {
	char	*file_name;
	char	*file_wd;
	char	*file_path;
	char	*file_rpath;

	int	 fd;
	int	 repo_fd;
	int	 file_type;
	int	 file_status;
	int	 file_flags;
	int	 in_attic;

	RCSNUM		*file_rcsrev;
	RCSFILE		*file_rcs;
	struct cvs_ent	*file_ent;
};

#define FILE_UNKNOWN		0
#define FILE_ADDED		1
#define FILE_REMOVED		2
#define FILE_MODIFIED		3
#define FILE_UPTODATE		4
#define FILE_LOST		5
#define FILE_CHECKOUT		6
#define FILE_MERGE		7
#define FILE_PATCH		8
#define FILE_REMOVE_ENTRY	9
#define FILE_CONFLICT		10
#define FILE_UNLINK		11

#define DIR_CREATE		12

#define FILE_SKIP		100

struct cvs_filelist {
	char	*file_path;
	TAILQ_ENTRY(cvs_filelist) flist;
};

TAILQ_HEAD(cvs_flisthead, cvs_filelist);

struct cvs_recursion;

#define CVS_DIR		1
#define CVS_FILE	2

TAILQ_HEAD(cvs_flist, cvs_file);

struct cvs_ignpat {
	char				ip_pat[MAXNAMLEN];
	int				ip_flags;
	TAILQ_ENTRY(cvs_ignpat)		ip_list;
};

TAILQ_HEAD(ignore_head, cvs_ignpat);

void	cvs_file_init(void);
void	cvs_file_ignore(const char *, struct ignore_head *);
void	cvs_file_classify(struct cvs_file *, const char *);
void	cvs_file_free(struct cvs_file *);
void	cvs_file_run(int, char **, struct cvs_recursion *);
void	cvs_file_walklist(struct cvs_flisthead *, struct cvs_recursion *);
void	cvs_file_walkdir(struct cvs_file *, struct cvs_recursion *);
void	cvs_file_freelist(struct cvs_flisthead *);
struct cvs_filelist *cvs_file_get(const char *, struct cvs_flisthead *);

int	cvs_file_chkign(const char *);
int	cvs_file_cmpname(const char *, const char *);
int	cvs_file_cmp(const char *, const char *);
int	cvs_file_copy(const char *, const char *);

struct cvs_file *cvs_file_get_cf(const char *, const char *, int, int);

#endif	/* FILE_H */

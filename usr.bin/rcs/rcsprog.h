/*	$OpenBSD: rcsprog.h,v 1.63 2010/07/23 21:46:05 ray Exp $	*/
/*
 * Copyright (c) 2005 Joris Vink <joris@openbsd.org>
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

#ifndef RCSPROG_H
#define RCSPROG_H

#include "rcs.h"
#include "rcsutil.h"
#include "worklist.h"
#include "xmalloc.h"

#define RCS_DEFAULT_SUFFIX	",v/"
#define RCS_TMPDIR_DEFAULT	"/tmp"

#define RCS_REV_BUFSZ	64
#define RCS_TIME_BUFSZ	64

/* flags specific to ci.c */
#define CI_SYMFORCE	(1<<0)
#define CI_DEFAULT	(1<<1)
#define CI_INIT		(1<<2)
#define CI_KEYWORDSCAN  (1<<3)
#define CI_SKIPDESC	(1<<4)

/* flags specific to co.c */
#define CO_ACLAPPEND	(1<<5)
#define CO_AUTHOR	(1<<6)
#define CO_LOCK		(1<<7)
#define CO_REVDATE	(1<<8)
#define CO_REVERT	(1<<9)
#define CO_STATE	(1<<10)
#define CO_UNLOCK	(1<<11)

/* flags specific to rcsprog.c */
#define RCSPROG_EFLAG	(1<<12)
#define RCSPROG_LFLAG	(1<<13)
#define RCSPROG_NFLAG	(1<<14)
#define RCSPROG_UFLAG	(1<<15)

/* flags shared between merge(1) and rcsmerge(1) */
#define MERGE_EFLAG	(1<<16)
#define MERGE_OFLAG	(1<<17)

/* shared flags  */
#define DESCRIPTION	(1<<18)
#define FORCE		(1<<19)
#define INTERACTIVE	(1<<20)
#define NEWFILE		(1<<21)
#define PIPEOUT		(1<<22)
#define PRESERVETIME	(1<<23)
#define QUIET		(1<<24)

extern char	*__progname;
extern const char	rcs_version[];

extern int	 rcs_optind;
extern char	*rcs_optarg;
extern char	*rcs_suffixes;
extern char	*rcs_tmpdir;
extern struct wklhead temp_files;

/* date.y */
time_t  date_parse(const char *);

/* ci.c */
int	checkin_main(int, char **);
void	checkin_usage(void);

/* co.c */
int	checkout_main(int, char **);
int	checkout_rev(RCSFILE *, RCSNUM *, const char *, int, const char *,
	    const char *, const char *, const char *);
void	checkout_usage(void);

/* ident.c */
int	ident_main(int, char **);
void	ident_usage(void);

/* merge.c */
int	merge_main(int, char **);
void	merge_usage(void);

/* rcsclean.c */
int	rcsclean_main(int, char **);
void	rcsclean_usage(void);

/* rcsdiff.c */
int	rcsdiff_main(int, char **);
void	rcsdiff_usage(void);

/* rcsmerge.c */
int	rcsmerge_main(int, char **);
void	rcsmerge_usage(void);

/* rcsprog.c */
int	build_cmd(char ***, char **, int);
int	rcs_getopt(int, char **, const char *);
int	rcs_main(int, char **);
void	rcs_usage(void);
void	(*usage)(void);

/* rlog.c */
int	rlog_main(int, char **);
void	rlog_usage(void);

#endif	/* RCSPROG_H */

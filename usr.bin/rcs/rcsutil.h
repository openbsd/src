/*	$OpenBSD: rcsutil.h,v 1.14 2010/07/23 08:31:19 ray Exp $	*/
/*
 * Copyright (c) 2006 Xavier Santolaria <xsa@openbsd.org>
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

#ifndef RCSUTIL_H
#define RCSUTIL_H

#include "rcs.h"

struct rcs_line {
	u_char			*l_line;
	int			 l_lineno;
	size_t			 l_len;
	TAILQ_ENTRY(rcs_line)	 l_list;
};

TAILQ_HEAD(tqh, rcs_line);

struct rcs_lines {
	int		 l_nblines;
	struct tqh	 l_lines;
};

struct rcs_argvector {
	char *str;
	char **argv;
};

/* rcsutil.c */
int			 rcs_getopt(int, char **, const char *);
void			 rcs_set_mtime(RCSFILE *, time_t);
int			 rcs_choosefile(const char *, char *, size_t);
time_t			 rcs_get_mtime(RCSFILE *);
RCSNUM			*rcs_getrevnum(const char *, RCSFILE *);
char			*rcs_prompt(const char *);
u_int			 rcs_rev_select(RCSFILE *, const char *);
int			 rcs_set_description(RCSFILE *, const char *);
void			 rcs_setrevstr(char **, char *);
void			 rcs_setrevstr2(char **, char **, char *);
BUF			*rcs_patchfile(u_char *, size_t, u_char *, size_t,
			    int (*p)(struct rcs_lines *,struct rcs_lines *));
struct rcs_lines	*rcs_splitlines(u_char *, size_t);
void			 rcs_freelines(struct rcs_lines *);
int			 rcs_yesno(int);
struct rcs_argvector	*rcs_strsplit(const char *, const char *);
void			 rcs_argv_destroy(struct rcs_argvector *);
void			 rcs_strip_suffix(char *);

#endif	/* RCSUTIL_H */

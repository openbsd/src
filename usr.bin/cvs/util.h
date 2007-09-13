/*	$OpenBSD: util.h,v 1.22 2007/09/13 13:10:57 tobias Exp $	*/
/*
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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

#ifndef UTIL_H
#define UTIL_H

void	  cvs_get_repository_path(const char *, char *, size_t);
void	  cvs_get_repository_name(const char *, char *, size_t);
void	  cvs_modetostr(mode_t, char *, size_t);
void	  cvs_strtomode(const char *, mode_t *);
void	  cvs_mkadmin(const char *, const char *, const char *,
	      char *, char *, int);
void	  cvs_mkpath(const char *, char *);
int	  cvs_cksum(const char *, char *, size_t);
int	  cvs_getargv(const char *, char **, int);
int	  cvs_chdir(const char *, int);
int	  cvs_rename(const char *, const char *);
int	  cvs_unlink(const char *);
int	  cvs_rmdir(const char *);
char	**cvs_makeargv(const char *, int *);
void	  cvs_freeargv(char **, int);
u_int	  cvs_revision_select(RCSFILE *, char *);

struct cvs_line {
	struct rcs_delta	*l_delta;
	u_char			*l_line;
	size_t			 l_len;
	int			 l_lineno;
	int			 l_lineno_orig;
	int			 l_needsfree;
	TAILQ_ENTRY(cvs_line)	 l_list;
};

TAILQ_HEAD(cvs_tqh, cvs_line);

struct cvs_lines {
	int		l_nblines;
	struct cvs_tqh	l_lines;
};

struct cvs_argvector {
	char *str;
	char **argv;
};

struct cvs_lines	*cvs_splitlines(u_char *, size_t);
void			cvs_freelines(struct cvs_lines *);
struct cvs_argvector	*cvs_strsplit(char *, const char *);
void			cvs_argv_destroy(struct cvs_argvector *);
int			cvs_yesno(void);

#endif	/* UTIL_H */

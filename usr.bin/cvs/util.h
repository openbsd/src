/*	$OpenBSD: util.h,v 1.4 2006/03/27 06:13:51 pat Exp $	*/
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

#if !defined(RCSPROG)


int	  cvs_readrepo(const char *, char *, size_t);
void	  cvs_modetostr(mode_t, char *, size_t);
void	  cvs_strtomode(const char *, mode_t *);
void	  cvs_splitpath(const char *, char *, size_t, char **);
int	  cvs_mkadmin(const char *, const char *, const char *, char *,
		char *, int);
int	  cvs_cksum(const char *, char *, size_t);
int	  cvs_exec(int, char **, int []);
int	  cvs_getargv(const char *, char **, int);
int	  cvs_chdir(const char *, int);
int	  cvs_rename(const char *, const char *);
int	  cvs_unlink(const char *);
int	  cvs_rmdir(const char *);
int	  cvs_create_dir(const char *, int, char *, char *);
char	 *cvs_rcs_getpath(CVSFILE *, char *, size_t);
char	**cvs_makeargv(const char *, int *);
void	  cvs_freeargv(char **, int);
void	  cvs_write_tagfile(char *, char *, int);
void	  cvs_parse_tagfile(char **, char **, int *);
size_t	  cvs_path_cat(const char *, const char *, char *, size_t);
time_t	  cvs_hack_time(time_t, int);

#endif	/* !RCSPROG */


struct cvs_line {
	char			*l_line;
	int			 l_lineno;
	TAILQ_ENTRY(cvs_line)	 l_list;
};

TAILQ_HEAD(cvs_tqh, cvs_line);

struct cvs_lines {
	int		l_nblines;
	char		*l_data;
	struct cvs_tqh	l_lines;
};

struct cvs_argvector {
	char *str;
	char **argv;
};

BUF			*cvs_patchfile(const char *, const char *,
			    int (*p)(struct cvs_lines *, struct cvs_lines *));
struct cvs_lines	*cvs_splitlines(const char *);
void			cvs_freelines(struct cvs_lines *);
int			cvs_yesno(void);
struct cvs_argvector	*cvs_strsplit(char *, const char *);

void			cvs_argv_destroy(struct cvs_argvector *);

#endif	/* UTIL_H */

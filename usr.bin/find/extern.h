/* *      $OpenBSD: extern.h,v 1.12 2002/02/16 21:27:46 millert Exp $*/
/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)extern.h	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>

void	 brace_subst(char *, char **, char *, int);
void	*emalloc(unsigned int);
PLAN	*find_create(char ***);
void	 find_execute(PLAN *, char **);
PLAN	*find_formplan(char **);
PLAN	*not_squish(PLAN *);
OPTION	*option(char *);
PLAN	*or_squish(PLAN *);
PLAN	*paren_squish(PLAN *);
struct stat;
void	 printlong(char *, char *, struct stat *);
int	 queryuser(char **);
void	 show_path(int);

PLAN	*c_amin(char *);
PLAN	*c_anewer(char *);
PLAN	*c_atime(char *);
PLAN	*c_cmin(char *);
PLAN	*c_cnewer(char *);
PLAN	*c_ctime(char *);
PLAN	*c_depth(void);
PLAN	*c_empty(void);
PLAN	*c_exec(char ***, int);
PLAN	*c_execdir(char ***);
PLAN	*c_flags(char *);
PLAN	*c_follow(void);
PLAN	*c_fstype(char *);
PLAN	*c_group(char *);
PLAN	*c_iname(char *);
PLAN	*c_inum(char *);
PLAN	*c_links(char *);
PLAN	*c_ls(void);
PLAN	*c_maxdepth(char *);
PLAN	*c_mindepth(char *);
PLAN	*c_mmin(char *);
PLAN	*c_name(char *);
PLAN	*c_newer(char *);
PLAN	*c_nogroup(void);
PLAN	*c_nouser(void);
PLAN	*c_path(char *);
PLAN	*c_perm(char *);
PLAN	*c_print(void);
PLAN	*c_print0(void);
PLAN	*c_prune(void);
PLAN	*c_size(char *);
PLAN	*c_type(char *);
PLAN	*c_user(char *);
PLAN	*c_xdev(void);
PLAN	*c_openparen(void);
PLAN	*c_closeparen(void);
PLAN	*c_mtime(char *);
PLAN	*c_not(void);
PLAN	*c_or(void);

extern int ftsoptions, isdeprecated, isdepth, isoutput, isxargs;

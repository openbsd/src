/* *      $OpenBSD: extern.h,v 1.15 2004/09/15 18:43:45 deraadt Exp $*/
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

PLAN	*c_amin(char *, char ***, int);
PLAN	*c_anewer(char *, char ***, int);
PLAN	*c_atime(char *, char ***, int);
PLAN	*c_cmin(char *, char ***, int);
PLAN	*c_cnewer(char *, char ***, int);
PLAN	*c_ctime(char *, char ***, int);
PLAN	*c_depth(char *, char ***, int);
PLAN	*c_empty(char *, char ***, int);
PLAN	*c_exec(char *, char ***, int);
PLAN	*c_execdir(char *, char ***, int);
PLAN	*c_flags(char *, char ***, int);
PLAN	*c_follow(char *, char ***, int);
PLAN	*c_fstype(char *, char ***, int);
PLAN	*c_group(char *, char ***, int);
PLAN	*c_iname(char *, char ***, int);
PLAN	*c_inum(char *, char ***, int);
PLAN	*c_links(char *, char ***, int);
PLAN	*c_ls(char *, char ***, int);
PLAN	*c_maxdepth(char *, char ***, int);
PLAN	*c_mindepth(char *, char ***, int);
PLAN	*c_mmin(char *, char ***, int);
PLAN	*c_name(char *, char ***, int);
PLAN	*c_newer(char *, char ***, int);
PLAN	*c_nogroup(char *, char ***, int);
PLAN	*c_nouser(char *, char ***, int);
PLAN	*c_path(char *, char ***, int);
PLAN	*c_perm(char *, char ***, int);
PLAN	*c_print(char *, char ***, int);
PLAN	*c_print0(char *, char ***, int);
PLAN	*c_prune(char *, char ***, int);
PLAN	*c_size(char *, char ***, int);
PLAN	*c_type(char *, char ***, int);
PLAN	*c_user(char *, char ***, int);
PLAN	*c_xdev(char *, char ***, int);
PLAN	*c_openparen(char *, char ***, int);
PLAN	*c_closeparen(char *, char ***, int);
PLAN	*c_mtime(char *, char ***, int);
PLAN	*c_not(char *, char ***, int);
PLAN	*c_or(char *, char ***, int);

extern int ftsoptions, isdepth, isoutput, isxargs;

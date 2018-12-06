/*	$OpenBSD: option.c,v 1.21 2018/12/06 17:45:14 tedu Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Cimarron D. Taylor of the University of California, Berkeley.
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

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "find.h"
#include "extern.h"

int typecompare(const void *, const void *);

/* NB: the following table must be sorted lexically. */
static OPTION options[] = {
	{ "!",		N_NOT,		c_not,		O_ZERO },
	{ "(",		N_OPENPAREN,	c_openparen,	O_ZERO },
	{ ")",		N_CLOSEPAREN,	c_closeparen,	O_ZERO },
	{ "-a",		N_AND,		NULL,		O_NONE },
	{ "-amin",	N_AMIN,		c_amin,		O_ARGV },
	{ "-and",	N_AND,		NULL,		O_NONE },
	{ "-anewer",	N_ANEWER,	c_anewer,	O_ARGV },
	{ "-atime",	N_ATIME,	c_atime,	O_ARGV },
	{ "-cmin",	N_CMIN,		c_cmin,		O_ARGV },
	{ "-cnewer",	N_CNEWER,	c_cnewer,	O_ARGV },
	{ "-ctime",	N_CTIME,	c_ctime,	O_ARGV },
	{ "-delete",	N_DELETE,	c_delete,	O_ZERO },
	{ "-depth",	N_DEPTH,	c_depth,	O_ZERO },
	{ "-empty",	N_EMPTY,	c_empty,	O_ZERO },
	{ "-exec",	N_EXEC,		c_exec,		O_ARGVP },
	{ "-execdir",	N_EXECDIR,	c_execdir,	O_ARGVP },
	{ "-flags",	N_FLAGS,	c_flags,	O_ARGV },
	{ "-follow",	N_FOLLOW,	c_follow,	O_ZERO },
	{ "-fstype",	N_FSTYPE,	c_fstype,	O_ARGV },
	{ "-group",	N_GROUP,	c_group,	O_ARGV },
	{ "-iname",	N_INAME,	c_iname,	O_ARGV },
	{ "-inum",	N_INUM,		c_inum,		O_ARGV },
	{ "-links",	N_LINKS,	c_links,	O_ARGV },
	{ "-ls",	N_LS,		c_ls,		O_ZERO },
	{ "-maxdepth",	N_MAXDEPTH,	c_maxdepth,	O_ARGV },
	{ "-mindepth",	N_MINDEPTH,	c_mindepth,	O_ARGV },
	{ "-mmin",	N_MMIN,		c_mmin,		O_ARGV },
	{ "-mount",	N_XDEV,		c_xdev,		O_ZERO },
	{ "-mtime",	N_MTIME,	c_mtime,	O_ARGV },
	{ "-name",	N_NAME,		c_name,		O_ARGV },
	{ "-newer",	N_NEWER,	c_newer,	O_ARGV },
	{ "-nogroup",	N_NOGROUP,	c_nogroup,	O_ZERO },
	{ "-not",	N_NOT,		c_not,		O_ZERO },
	{ "-nouser",	N_NOUSER,	c_nouser,	O_ZERO },
	{ "-o",		N_OR,		c_or,		O_ZERO },
	{ "-ok",	N_OK,		c_exec,		O_ARGVP },
	{ "-or",	N_OR,		c_or,		O_ZERO },
	{ "-path", 	N_PATH,		c_path,		O_ARGV },
	{ "-perm",	N_PERM,		c_perm,		O_ARGV },
	{ "-print",	N_PRINT,	c_print,	O_ZERO },
	{ "-print0",	N_PRINT0,	c_print0,	O_ZERO },
	{ "-prune",	N_PRUNE,	c_prune,	O_ZERO },
	{ "-size",	N_SIZE,		c_size,		O_ARGV },
	{ "-type",	N_TYPE,		c_type,		O_ARGV },
	{ "-user",	N_USER,		c_user,		O_ARGV },
	{ "-xdev",	N_XDEV,		c_xdev,		O_ZERO },
};

/*
 * find_create --
 *	create a node corresponding to a command line argument.
 *
 * TODO:
 *	add create/process function pointers to node, so we can skip
 *	this switch stuff.
 */
PLAN *
find_create(char ***argvp)
{
	OPTION *p;
	PLAN *new;
	char **argv;

	argv = *argvp;

	if ((p = option(*argv)) == NULL)
		errx(1, "%s: unknown option", *argv);
	++argv;
	if (p->flags & (O_ARGV|O_ARGVP) && !*argv)
		errx(1, "%s: requires additional arguments", *--argv);

	switch(p->flags) {
	case O_NONE:
		new = NULL;
		break;
	case O_ZERO:
		new = (p->create)(NULL, NULL, 0);
		break;
	case O_ARGV:
		new = (p->create)(*argv++, NULL, 0);
		break;
	case O_ARGVP:
		new = (p->create)(NULL, &argv, p->token == N_OK);
		mayexecve = 1;
		break;
	default:
		abort();
	}
	*argvp = argv;
	return (new);
}

OPTION *
option(char *name)
{
	OPTION tmp;

	tmp.name = name;
	return ((OPTION *)bsearch(&tmp, options,
	    sizeof(options)/sizeof(OPTION), sizeof(OPTION), typecompare));
}

int
typecompare(const void *a, const void *b)
{
	return (strcmp(((OPTION *)a)->name, ((OPTION *)b)->name));
}

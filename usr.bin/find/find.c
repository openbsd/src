/*	$OpenBSD: find.c,v 1.23 2018/08/01 06:39:58 tb Exp $	*/

/*-
 * Copyright (c) 1991, 1993
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
#include <errno.h>
#include <fts.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int	mayexecve;

#include "find.h"

/*
 * find_formplan --
 *	process the command line and create a "plan" corresponding to the
 *	command arguments.
 */
PLAN *
find_formplan(char **argv)
{
	PLAN *plan, *tail, *new;

	/*
	 * for each argument in the command line, determine what kind of node
	 * it is, create the appropriate node type and add the new plan node
	 * to the end of the existing plan.  The resulting plan is a linked
	 * list of plan nodes.  For example, the string:
	 *
	 *	% find . -name foo -newer bar -print
	 *
	 * results in the plan:
	 *
	 *	[-name foo]--> [-newer bar]--> [-print]
	 *
	 * in this diagram, `[-name foo]' represents the plan node generated
	 * by c_name() with an argument of foo and `-->' represents the
	 * plan->next pointer.
	 */
	for (plan = tail = NULL; *argv;) {
		if (!(new = find_create(&argv)))
			continue;
		if (plan == NULL)
			tail = plan = new;
		else {
			tail->next = new;
			tail = new;
		}
	}
    
	/*
	 * if the user didn't specify one of -delete, -exec, -execdir,
	 * -ls, -ok, -print or -print0, then -print is assumed so we
	 * bracket the current expression with parens, if necessary,
	 * and add a -print node on the end.
	 */
	if (!isoutput) {
		if (plan == NULL) {
			new = c_print(NULL, NULL, 0);
			tail = plan = new;
		} else {
			new = c_openparen(NULL, NULL, 0);
			new->next = plan;
			plan = new;
			new = c_closeparen(NULL, NULL, 0);
			tail->next = new;
			tail = new;
			new = c_print(NULL, NULL, 0);
			tail->next = new;
			tail = new;
		}
	}
    
	/*
	 * the command line has been completely processed into a search plan
	 * except for the (, ), !, and -o operators.  Rearrange the plan so
	 * that the portions of the plan which are affected by the operators
	 * are moved into operator nodes themselves.  For example:
	 *
	 *	[!]--> [-name foo]--> [-print]
	 *
	 * becomes
	 *
	 *	[! [-name foo] ]--> [-print]
	 *
	 * and
	 *
	 *	[(]--> [-depth]--> [-name foo]--> [)]--> [-print]
	 *
	 * becomes
	 *
	 *	[expr [-depth]-->[-name foo] ]--> [-print]
	 *
	 * operators are handled in order of precedence.
	 */

	plan = paren_squish(plan);		/* ()'s */
	plan = not_squish(plan);		/* !'s */
	plan = or_squish(plan);			/* -o's */
	return (plan);
}
 
FTS *tree;			/* pointer to top of FTS hierarchy */

/*
 * find_execute --
 *	take a search plan and an array of search paths and executes the plan
 *	over all FTSENT's returned for the given search paths.
 */

FTSENT *entry;			/* shared with SIGINFO handler */

int
find_execute(PLAN *plan,	/* search plan */
    char **paths)		/* array of pathnames to traverse */
{
	sigset_t fullset, oset;
	int r, rval;
	PLAN *p;

	if (mayexecve == 0) {
		if (isdelete) {
			if (pledge("stdio rpath cpath getpw", NULL) == -1)
				err(1, "pledge");
		} else {
			if (pledge("stdio rpath getpw", NULL) == -1)
				err(1, "pledge");
		}
	} else {
		if (isdelete) {
			if (pledge("stdio rpath cpath getpw proc exec", NULL)
			    == -1)
				err(1, "pledge");
		} else {
			if (pledge("stdio rpath getpw proc exec", NULL) == -1)
				err(1, "pledge");
		}
	}

	rval = 0;
    
	if (!(tree = fts_open(paths, ftsoptions, NULL)))
		err(1, "fts_open");

	sigfillset(&fullset);
	for (;;) {
		(void)sigprocmask(SIG_BLOCK, &fullset, &oset);
		entry = fts_read(tree);
		(void)sigprocmask(SIG_SETMASK, &oset, NULL);
		if (entry == NULL) {
			if (errno)
				err(1, "fts_read");
			break;
		}

		switch (entry->fts_info) {
		case FTS_D:
			if (isdepth)
				continue;
			break;
		case FTS_DP:
			if (!isdepth)
				continue;
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			(void)fflush(stdout);
			warnc(entry->fts_errno, "%s", entry->fts_path);
			rval = 1;
			continue;
		}
#define	BADCH	" \t\n\\'\""
		if (isxargs && strpbrk(entry->fts_path, BADCH)) {
			(void)fflush(stdout);
			warnx("%s: illegal path", entry->fts_path);
			rval = 1;
			continue;
		}

		/*
		 * Call all the functions in the execution plan until one is
		 * false or all have been executed.  This is where we do all
		 * the work specified by the user on the command line.
		 */
		for (p = plan; p && (p->eval)(p, entry); p = p->next)
		    ;
	}
	(void)fts_close(tree);

	/*
	 * Cleanup any plans with leftover state.
	 * Keep the last non-zero return value.
	 */
	if ((r = find_traverse(plan, plan_cleanup, NULL)) != 0)
		rval = r;
	return (rval);
}

/*
 * find_traverse --
 *	traverse the plan tree and execute func() on all plans.  This
 *	does not evaluate each plan's eval() function; it is intended
 *	for operations that must run on all plans, such as state
 *	cleanup.
 *
 *	If any func() returns non-zero, then so will find_traverse().
 */
int
find_traverse(PLAN *plan, int (*func)(PLAN *, void *), void *arg)
{
	PLAN *p;
	int r, rval;

	rval = 0;
	for (p = plan; p; p = p->next) {
		if ((r = func(p, arg)) != 0)
			rval = r;
		if (p->type == N_EXPR || p->type == N_OR) {
			if (p->p_data[0])
				if ((r = find_traverse(p->p_data[0],
					    func, arg)) != 0)
					rval = r;
			if (p->p_data[1])
				if ((r = find_traverse(p->p_data[1],
					    func, arg)) != 0)
					rval = r;
		}
	}
	return rval;
}

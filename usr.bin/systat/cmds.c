/*	$OpenBSD: cmds.c,v 1.16 2007/05/29 20:56:54 tedu Exp $	*/
/*	$NetBSD: cmds.c,v 1.4 1996/05/10 23:16:32 thorpej Exp $	*/

/*-
 * Copyright (c) 1980, 1992, 1993
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)cmds.c	8.2 (Berkeley) 4/29/95";
#endif
static char rcsid[] = "$OpenBSD: cmds.c,v 1.16 2007/05/29 20:56:54 tedu Exp $";
#endif /* not lint */

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include "systat.h"
#include "extern.h"

void
command(char *cmd)
{
	struct cmdtab *p;
	char *cp;
	double interval;
	sigset_t mask, omask;

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	for (cp = cmd; *cp && !isspace(*cp); cp++)
		;
	if (*cp)
		*cp++ = '\0';
	if (*cmd == '\0')
		return;
	for (; isspace(*cp); cp++)
		;
	if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0)
		die();
	if (strcmp(cmd, "load") == 0) {
		load();
		goto done;
	}
	if (strcmp(cmd, "stop") == 0) {
		alarm(0);
		mvaddstr(CMDLINE, 0, "Refresh disabled.");
		clrtoeol();
		goto done;
	}
	if (strcmp(cmd, "help") == 0) {
		size_t len;
		int column = 0;

		move(CMDLINE, column);
		for (p = cmdtab; p->c_name; p++) {
			len = strlen(p->c_name);
			if (column + len > COLS)
				break;
			addstr(p->c_name);
			column += len;
			if (column + 1 < COLS)
				addch(' ');
		}
		clrtoeol();
		goto done;
	}
	interval = strtod(cmd, NULL);
	if (interval < 0.09 &&
	    (strcmp(cmd, "start") == 0 || strcmp(cmd, "interval") == 0)) {
		interval = *cp ? strtod(cp, NULL) : naptime;
		if (interval < 0.09) {
			error("%d: bad interval.", interval);
			goto done;
		}
	}
	if (interval >= 0.09) {
		alarm(0);
		naptime = interval;
		display();
		status();
		goto done;
	}
	p = lookup(cmd);
	if (p == (struct cmdtab *)-1) {
		error("%s: Ambiguous command.", cmd);
		goto done;
	}
	if (p) {
		if (curcmd == p)
			goto done;
		alarm(0);
		(*curcmd->c_close)(wnd);
		wnd = (*p->c_open)();
		if (wnd == 0) {
			error("Couldn't open new display");
			wnd = (*curcmd->c_open)();
			if (wnd == 0) {
				error("Couldn't change back to previous cmd");
				exit(1);
			}
			p = curcmd;
		}
		if ((p->c_flags & CF_INIT) == 0) {
			if ((*p->c_init)())
				p->c_flags |= CF_INIT;
			else
				goto done;
		}
		curcmd = p;
		labels();
		display();
		status();
		goto done;
	}
	if (curcmd->c_cmd == 0 || !(*curcmd->c_cmd)(cmd, cp))
		error("%s: Unknown command.", cmd);
done:
	sigprocmask(SIG_SETMASK, &omask, NULL);
}

struct cmdtab *
lookup(char *name)
{
	char *p, *q;
	struct cmdtab *c, *found;
	int nmatches, longest;

	longest = 0;
	nmatches = 0;
	found = (struct cmdtab *) 0;
	for (c = cmdtab; (p = c->c_name); c++) {
		for (q = name; *q == *p++; q++)
			if (*q == 0)		/* exact match? */
				return (c);
		if (!*q) {			/* the name was a prefix */
			if (q - name > longest) {
				longest = q - name;
				nmatches = 1;
				found = c;
			} else if (q - name == longest)
				nmatches++;
		}
	}
	if (nmatches > 1)
		return ((struct cmdtab *)-1);
	return (found);
}

void
status(void)
{

	error("Showing %s, refresh every %.1f seconds.",
	    curcmd->c_name, naptime);
}

int
prefix(char *s1, char *s2)
{

	while (*s1 == *s2) {
		if (*s1 == '\0')
			return (1);
		s1++, s2++;
	}
	return (*s1 == '\0');
}

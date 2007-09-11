/*	$OpenBSD: disks.c,v 1.17 2007/09/11 15:47:17 gilles Exp $	*/
/*	$NetBSD: disks.c,v 1.4 1996/05/10 23:16:33 thorpej Exp $	*/

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
static char sccsid[] = "@(#)disks.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: disks.c,v 1.17 2007/09/11 15:47:17 gilles Exp $";
#endif /* not lint */

#include <string.h>
#include <ctype.h>
#include <signal.h>
#include "systat.h"
#include "extern.h"
static void dkselect(char *args, int truefalse, int selections[]);


int
dkcmd(char *cmd, char *args)
{
	if (prefix(cmd, "display") || prefix(cmd, "add")) {
		dkselect(args, 1, dk_select);
		return (1);
	}
	if (prefix(cmd, "ignore") || prefix(cmd, "delete")) {
		dkselect(args, 0, dk_select);
		return (1);
	}
	if (prefix(cmd, "drives")) {
		int i;

		move(CMDLINE, 0);
		clrtoeol();
		for (i = 0; i < dk_ndrive; i++)
			printw("%s ", dr_name[i]);
		return (1);
	}
	return (0);
}

static void
dkselect(char *args, int truefalse, int selections[])
{
	char *cp;
	int i;

	args[strcspn(args, "\n")] = '\0';

	for (;;) {
		for (cp = args; isspace(*cp); cp++)
			;
		args = cp;
		for (; *cp && !isspace(*cp); cp++)
			;
		if (*cp)
			*cp++ = '\0';
		if (cp - args == 0)
			break;
		for (i = 0; i < dk_ndrive; i++)
			if (strcmp(args, dr_name[i]) == 0) {
				selections[i] = truefalse;
				break;
			}
		if (i >= dk_ndrive)
			error("%s: unknown drive", args);
		args = cp;
	}
}

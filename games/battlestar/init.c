/*	$OpenBSD: init.c,v 1.5 1998/09/13 01:30:32 pjanzen Exp $	*/
/*	$NetBSD: init.c,v 1.4 1995/03/21 15:07:35 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1993
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)init.c	8.1 (Berkeley) 5/31/93";
#else
static char rcsid[] = "$OpenBSD: init.c,v 1.5 1998/09/13 01:30:32 pjanzen Exp $";
#endif
#endif /* not lint */

#include "extern.h"

void
initialize(startup)
	char    startup;
{
	struct objs *p;

	puts("Version 4.2, fall 1984.");
	puts("First Adventure game written by His Lordship, the honorable");
	puts("Admiral D.W. Riggle\n");
	location = dayfile;
	srandom(getpid());
	getutmp(uname);
	wordinit();
	if (startup) {
		direction = NORTH;
		ourtime = 0;
		snooze = CYCLE * 1.5;
		position = 22;
		setbit(wear, PAJAMAS);
		fuel = TANKFULL;
		torps = TORPEDOES;
		for (p = dayobjs; p->room != 0; p++)
			setbit(location[p->room].objects, p->obj);
	} else
		restore();
	wiz = wizard(uname);
	signal(SIGINT, die);
}

void
getutmp(uname)
	char   *uname;
{
	struct passwd *ptr;

	ptr = getpwuid(getuid());
	strcpy(uname, ptr ? ptr->pw_name : "");
}

char   *list[] = {	/* hereditary wizards */
	"riggle",
	"chris",
	"edward",
	"comay",
	"yee",
	"dmr",
	"ken",
	0
};

char   *badguys[] = {
	"wnj",
	"root",
	"ted",
	0
};

int
wizard(uname)
	char   *uname;
{
	int     flag;

	if ((flag = checkout(uname)) != 0)
		printf("You are the Great wizard %s.\n", uname);
	return flag;
}

int
checkout(uname)
	char   *uname;
{
	char  **ptr;

	for (ptr = list; *ptr; ptr++)
		if (strcmp(*ptr, uname) == 0)
			return 1;
	for (ptr = badguys; *ptr; ptr++)
		if (strcmp(*ptr, uname) == 0) {
			printf("You are the Poor anti-wizard %s.  Good Luck!\n",
			    uname);
			if (location != NULL) {
				CUMBER = 3;
				WEIGHT = 9;	/* that'll get him! */
				ourclock = 10;
				setbit(location[7].objects, WOODSMAN);	/* viper room */
				setbit(location[20].objects, WOODSMAN);	/* laser " */
				setbit(location[13].objects, DARK);	/* amulet " */
				setbit(location[8].objects, ELF);	/* closet */
			}
			return 0;	/* anything else, Chris? */
		}
	return 0;
}

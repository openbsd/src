/*	$OpenBSD: finger.c,v 1.10 2000/01/12 11:08:05 aaron Exp $	*/

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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

/*
 * Luke Mewburn <lukem@netbsd.org> added the following on 961121:
 *    - mail status ("No Mail", "Mail read:...", or "New Mail ...,
 *	Unread since ...".)
 *    - 4 digit phone extensions (3210 is printed as x3210.)
 *    - host/office toggling in short format with -h & -o.
 *    - short day names (`Tue' printed instead of `Jun 21' if the
 *	login time is < 6 days.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1989 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)finger.c	5.22 (Berkeley) 6/29/90";*/
static char rcsid[] = "$OpenBSD: finger.c,v 1.10 2000/01/12 11:08:05 aaron Exp $";
#endif /* not lint */

/*
 * Finger prints out information about users.  It is not portable since
 * certain fields (e.g. the full user name, office, and phone numbers) are
 * extracted from the gecos field of the passwd file which other UNIXes
 * may not have or may use for other things.
 *
 * There are currently two output formats; the short format is one line
 * per user and displays login name, tty, login time, real name, idle time,
 * and either remote host information (default) or office location/phone
 * number, depending on if -h or -o is used respectively.
 * The long format gives the same information (in a more legible format) as
 * well as home directory, shell, mail info, and .plan/.project files.
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <err.h>
#include "finger.h"
#include "extern.h"

time_t now;
int entries, lflag, sflag, mflag, oflag, pplan, Mflag;
char tbuf[1024];

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *__progname;
	int ch;
	char domain[MAXHOSTNAMELEN];
	struct stat sb;

	oflag = 1;		/* default to old "office" behavior */

	while ((ch = getopt(argc, argv, "lmMpsho")) != -1)
		switch(ch) {
		case 'l':
			lflag = 1;		/* long format */
			break;
		case 'm':
			mflag = 1;		/* force exact match of names */
			break;
		case 'M':
			Mflag = 1;		/* allow name matching */
			break;
		case 'p':
			pplan = 1;		/* don't show .plan/.project */
			break;
		case 's':
			sflag = 1;		/* short format */
			break;
		case 'h':
			oflag = 0;		/* remote host info */
			break;
		case 'o':
			oflag = 1;		/* office info */
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: %s [-lmMpsho] [login ...]\n", __progname);
			exit(1);
		}
	argc -= optind;
	argv += optind;

	/* If a domainname is set, increment mflag. */
	if ((getdomainname(&domain, sizeof(domain)) == 0) && domain[0])
		mflag++;
	/* If _PATH_MP_DB is larger than 1MB, increment mflag. */
	if (stat(_PATH_MP_DB, &sb) == 0) {
		if (sb.st_size > 1048576)
			mflag++;
	}

	(void)time(&now);
	setpassent(1);
	if (!*argv) {
		/*
		 * Assign explicit "small" format if no names given and -l
		 * not selected.  Force the -s BEFORE we get names so proper
		 * screening will be done.
		 */
		if (!lflag)
			sflag = 1;	/* if -l not explicit, force -s */
		loginlist();
		if (entries == 0)
			(void)printf("No one logged on.\n");
	} else {
		userlist(argc, argv);
		/*
		 * Assign explicit "large" format if names given and -s not
		 * explicitly stated.  Force the -l AFTER we get names so any
		 * remote finger attempts specified won't be mishandled.
		 */
		if (!sflag)
			lflag = 1;	/* if -s not explicit, force -l */
	}
	if (entries != 0) {
		if (lflag)
			lflag_print();
		else
			sflag_print();
	}
	exit(0);
}

void
loginlist()
{
	PERSON *pn;
	struct passwd *pw;
	struct utmp user;
	char name[UT_NAMESIZE + 1];

	if (!freopen(_PATH_UTMP, "r", stdin))
		err(2, _PATH_UTMP);
	name[UT_NAMESIZE] = '\0';
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL) {
			bcopy(user.ut_name, name, UT_NAMESIZE);
			if ((pw = getpwnam(name)) == NULL)
				continue;
			pn = enter_person(pw);
		}
		enter_where(&user, pn);
	}
	for (pn = phead; lflag && pn != NULL; pn = pn->next)
		enter_lastlog(pn);
}

void
userlist(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	register PERSON *pn;
	PERSON *nethead, **nettail;
	struct utmp user;
	struct passwd *pw;
	int dolocal, *used;

	if (!(used = (int *)calloc((u_int)argc, (u_int)sizeof(int))))
		err(2, "malloc");

	/* pull out all network requests */
	for (i = 0, dolocal = 0, nettail = &nethead; i < argc; i++) {
		if (!strchr(argv[i], '@')) {
			dolocal = 1;
			continue;
		}
		pn = palloc();
		*nettail = pn;
		nettail = &pn->next;
		pn->name = argv[i];
		used[i] = -1;
	}
	*nettail = NULL;

	if (!dolocal)
		goto net;

	/*
	 * traverse the list of possible login names and check the login name
	 * and real name against the name specified by the user.
	 */
	if ((mflag - Mflag) > 0) {
		for (i = 0; i < argc; i++)
			if (used[i] >= 0 && (pw = getpwnam(argv[i]))) {
				enter_person(pw);
				used[i] = 1;
			}
	} else while ((pw = getpwent()) != NULL)
		for (i = 0; i < argc; i++)
			if (used[i] >= 0 &&
			    (!strcasecmp(pw->pw_name, argv[i]) ||
			    match(pw, argv[i]))) {
				enter_person(pw);
				used[i] = 1;
			}

	/* list errors */
	for (i = 0; i < argc; i++)
		if (!used[i])
			warnx("%s: no such user.", argv[i]);

	/* handle network requests */
net:	for (pn = nethead; pn; pn = pn->next) {
		netfinger(pn->name);
		if (pn->next || entries)
			putchar('\n');
	}

	if (entries == 0)
		return;

	/*
	 * Scan thru the list of users currently logged in, saving
	 * appropriate data whenever a match occurs.
	 */
	if (!freopen(_PATH_UTMP, "r", stdin))
		err(1, _PATH_UTMP);
	while (fread((char *)&user, sizeof(user), 1, stdin) == 1) {
		if (!user.ut_name[0])
			continue;
		if ((pn = find_person(user.ut_name)) == NULL)
			continue;
		enter_where(&user, pn);
	}
	for (pn = phead; pn != NULL; pn = pn->next)
		enter_lastlog(pn);
}

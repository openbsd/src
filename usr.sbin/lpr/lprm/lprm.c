/*	$OpenBSD: lprm.c,v 1.13 2002/12/08 16:50:07 millert Exp $	*/
/*	$$NetBSD: lprm.c,v 1.9 1999/08/16 03:12:32 simonb Exp $	*/

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
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
static const char copyright[] =
"@(#) Copyright (c) 1983, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)lprm.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] = "$OpenBSD: lprm.c,v 1.13 2002/12/08 16:50:07 millert Exp $";
#endif
#endif /* not lint */

/*
 * lprm - remove the current user's spool entry
 *
 * lprm [-] [[job #] [user] ...]
 *
 * Using information in the lock file, lprm will kill the
 * currently active daemon (if necessary), remove the associated files,
 * and startup a new daemon.  Privileged users may remove anyone's spool
 * entries, otherwise one can only remove their own.
 */

#include <sys/param.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "lp.h"
#include "lp.local.h"

/*
 * Stuff for handling job specifications
 */
char	*person;		/* name of person doing lprm */
int	 requ[MAXREQUESTS];	/* job number of spool entries */
int	 requests;		/* # of spool requests */
char	*user[MAXUSERS];	/* users to process */
int	 users;			/* # of users in user array */
volatile sig_atomic_t gotintr;	/* set when we receive SIGINT */
static char luser[MAXLOGNAME];	/* buffer for person */

static __dead void usage(void);

int
main(int argc, char **argv)
{
	struct passwd *p;
	char *cp;
	long l;
	int ch;

	effective_uid = geteuid();
	real_uid = getuid();
	effective_gid = getegid();
	real_gid = getgid();
	PRIV_END;	/* be safe */

	gethostname(host, sizeof(host));
	openlog("lprm", 0, LOG_LPR);
	if ((p = getpwuid(real_uid)) == NULL)
		fatal("Who are you?");
	if (strlen(p->pw_name) >= sizeof(luser))
		fatal("Your name is too long");
	strlcpy(luser, p->pw_name, sizeof(luser));
	person = luser;
	while ((ch = getopt(argc, argv, "P:w:-")) != -1) {
		switch (ch) {
		case '-':
			users = -1;
			break;
		case 'P':
			printer = optarg;
			break;
		case 'w':
			l = strtol(optarg, &cp, 10);
			if (*cp != '\0' || l < 0 || l >= INT_MAX)
				errx(1, "wait time must be postive integer: %s",
				    optarg);
			wait_time = (u_int)l;
			if (wait_time < 30)
				warnx("warning: wait time less than 30 seconds");
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (printer == NULL && (printer = getenv("PRINTER")) == NULL)
		printer = DEFLP;
	if (users < 0 && argc != 0)
		usage();
	while (argc > 0) {
		if (isdigit(*argv[0])) {
			if (requests >= MAXREQUESTS)
				fatal("Too many requests");
			requ[requests++] = atoi(argv[0]);
		} else {
			if (users >= MAXUSERS)
				fatal("Too many users");
			user[users++] = argv[0];
		}
		argc--;
		argv++;
	}

	rmjob();
	exit(0);
}

static __dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-] [-Pprinter] [[job #] [user] ...]\n",
	    __progname);
	exit(2);
}

/*	$NetBSD: who.c,v 1.4 1994/12/07 04:28:49 jtc Exp $	*/

/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Michael Fischbein.
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
static char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)who.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$NetBSD: who.c,v 1.4 1994/12/07 04:28:49 jtc Exp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <err.h>

void output __P((struct utmp *));
void who_am_i __P((FILE *));
void usage __P((void));
int only_current_term;		/* show info about the current terminal only */
int show_term;			/* show term state */
int show_idle;			/* show idle time */

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct utmp usr;
	FILE *ufp, *file();
	int c;

	while ((c = getopt(argc, argv, "mTu")) != -1) {
		switch (c) {
		case 'm':
			only_current_term = 1;
			break;
		case 'T':
			show_term = 1;
			break;
		case 'u':
			show_idle = 1;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (chdir("/dev")) {
		err(1, "cannot change directory to /dev");
		/* NOTREACHED */
	}

	switch (argc) {
	case 0:					/* who */
		ufp = file(_PATH_UTMP);

		if (only_current_term) {
			who_am_i(ufp);
		} else {
			/* only entries with both name and line fields */
			while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
				if (*usr.ut_name && *usr.ut_line)
					output(&usr);
		}
		break;
	case 1:					/* who utmp_file */
		ufp = file(*argv);

		if (only_current_term) {
			who_am_i(ufp);
		} else {
			/* all entries */
			while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
				output(&usr);
		}
		break;
	case 2:					/* who am i */
		ufp = file(_PATH_UTMP);
		who_am_i(ufp);
		break;
	default:
		usage();
		/* NOTREACHED */
	}
	exit(0);
}

void
who_am_i(ufp)
	FILE *ufp;
{
	struct utmp usr;
	struct passwd *pw;
	register char *p;
	char *t;

	/* search through the utmp and find an entry for this tty */
	if (p = ttyname(0)) {
		/* strip any directory component */
		if (t = rindex(p, '/'))
			p = t + 1;
		while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
			if (usr.ut_name && !strcmp(usr.ut_line, p)) {
				output(&usr);
				return;
			}
		/* well, at least we know what the tty is */
		(void)strncpy(usr.ut_line, p, UT_LINESIZE);
	} else
		(void)strcpy(usr.ut_line, "tty??");

	pw = getpwuid(getuid());
	(void)strncpy(usr.ut_name, pw ? pw->pw_name : "?", UT_NAMESIZE);
	(void)time(&usr.ut_time);
	*usr.ut_host = '\0';
	output(&usr);
}

void
output(up)
	struct utmp *up;
{
	struct stat sb;
	char line[sizeof (up->ut_line) + 1];
	char state;
	static time_t now = 0;
	time_t idle;

	if (show_term || show_idle) {
		if (now == 0)
			time(&now);
		
		strncpy (line, up->ut_line, sizeof (up->ut_line));
		line[sizeof (up->ut_line)] = '\0';

		if (stat(line, &sb) == 0) {
			state = (sb.st_mode & 020) ? '+' : '-';
			idle = now - sb.st_atime;
		} else {
			state = '?';
			idle = 0;
		}
		
	}

	(void)printf("%-*.*s ", UT_NAMESIZE, UT_NAMESIZE, up->ut_name);

	if (show_term) {
		(void)printf("%c ", state);
	}

	(void)printf("%-*.*s ", UT_LINESIZE, UT_LINESIZE, up->ut_line);
	(void)printf("%.12s ", ctime(&up->ut_time) + 4);

	if (show_idle) {
		if (idle < 60) 
			(void)printf("  .   ");
		else if (idle < (24 * 60 * 60))
			(void)printf("%02d:%02d ", 
				     (idle / (60 * 60)),
				     (idle % (60 * 60)) / 60);
		else
			(void)printf(" old  ");
	}
	
	if (*up->ut_host)
		printf("\t(%.*s)", UT_HOSTSIZE, up->ut_host);
	(void)putchar('\n');
}

FILE *
file(name)
	char *name;
{
	FILE *ufp;

	if (!(ufp = fopen(name, "r"))) {
		err(1, "%s", name);
		/* NOTREACHED */
	}
	return(ufp);
}

void
usage()
{
	(void)fprintf(stderr, "usage: who [-mTu] [ file ]\n       who am i\n");
	exit(1);
}

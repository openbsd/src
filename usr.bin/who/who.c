/*	$OpenBSD: who.c,v 1.18 2009/10/27 23:59:50 deraadt Exp $	*/
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
#include <paths.h>
#include <pwd.h>
#include <utmp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <err.h>
#include <locale.h>

void  output(struct utmp *);
void  output_labels(void);
void  who_am_i(FILE *);
void  usage(void);
FILE *file(char *);

int only_current_term;		/* show info about the current terminal only */
int show_term;			/* show term state */
int show_idle;			/* show idle time */
int show_labels;		/* show column labels */
int show_quick;			/* quick, names only */

#define NAME_WIDTH	8
#define HOST_WIDTH	32

int
main(int argc, char *argv[])
{
	struct utmp usr;
	FILE *ufp;
	int c;

	setlocale(LC_ALL, "");

	only_current_term = show_term = show_idle = show_labels = 0;
	show_quick = 0;
	while ((c = getopt(argc, argv, "HmqTu")) != -1) {
		switch (c) {
		case 'H':
			show_labels = 1;
			break;
		case 'm':
			only_current_term = 1;
			break;
		case 'q':
			show_quick = 1;
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

	if (show_quick) {
		only_current_term = show_term = show_idle = show_labels = 0;
	}

	if (show_labels)
		output_labels();

	switch (argc) {
	case 0:					/* who */
		ufp = file(_PATH_UTMP);

		if (only_current_term) {
			who_am_i(ufp);
		} else if (show_quick) {
			int count = 0;
	
			while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1) {
				if (*usr.ut_name && *usr.ut_line) {
					(void)printf("%-*.*s ", NAME_WIDTH,
						UT_NAMESIZE, usr.ut_name);
					if ((++count % 8) == 0)
						(void) printf("\n");
				}
			}
			if (count % 8)
				(void) printf("\n");
			(void) printf ("# users=%d\n", count);
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
		} else if (show_quick) {
			int count = 0;

			while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1) {
				if (*usr.ut_name && *usr.ut_line) {
					(void)printf("%-*.*s ", NAME_WIDTH,
						UT_NAMESIZE, usr.ut_name);
					if ((++count % 8) == 0)
						(void) printf("\n");
				}
			}
			if (count % 8)
				(void) printf("\n");
			(void) printf ("# users=%d\n", count);
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
who_am_i(FILE *ufp)
{
	struct utmp usr;
	struct passwd *pw;
	char *p;
	char *t;

	/* search through the utmp and find an entry for this tty */
	if ((p = ttyname(0))) {
		/* strip any directory component */
		if ((t = strrchr(p, '/')))
			p = t + 1;
		while (fread((char *)&usr, sizeof(usr), 1, ufp) == 1)
			if (*usr.ut_name && !strcmp(usr.ut_line, p)) {
				output(&usr);
				return;
			}
		/* well, at least we know what the tty is */
		(void)strncpy(usr.ut_line, p, UT_LINESIZE);
	} else
		(void)strncpy(usr.ut_line, "tty??", UT_LINESIZE);

	pw = getpwuid(getuid());
	(void)strncpy(usr.ut_name, pw ? pw->pw_name : "?", UT_NAMESIZE);
	(void)time(&usr.ut_time);
	*usr.ut_host = '\0';
	output(&usr);
}

void
output(struct utmp *up)
{
	struct stat sb;
	char line[sizeof(_PATH_DEV) + sizeof (up->ut_line)];
	char state = '?';
	static time_t now = 0;
	time_t idle = 0;

	if (show_term || show_idle) {
		if (now == 0)
			time(&now);
		
		memset(line, 0, sizeof line);
		strlcpy(line, _PATH_DEV, sizeof line);
		strlcat(line, up->ut_line, sizeof line);

		if (stat(line, &sb) == 0) {
			state = (sb.st_mode & 020) ? '+' : '-';
			idle = now - sb.st_atime;
		} else {
			state = '?';
			idle = 0;
		}
		
	}

	(void)printf("%-*.*s ", NAME_WIDTH, UT_NAMESIZE, up->ut_name);

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
		printf("  (%.*s)", HOST_WIDTH, up->ut_host);
	(void)putchar('\n');
}

void
output_labels(void)
{
	(void)printf("%-*.*s ", NAME_WIDTH, UT_NAMESIZE, "USER");

	if (show_term)
		(void)printf("S ");

	(void)printf("%-*.*s ", UT_LINESIZE, UT_LINESIZE, "LINE");
	(void)printf("WHEN         ");

	if (show_idle)
		(void)printf("IDLE  ");

	(void)printf("  %.*s", HOST_WIDTH, "FROM");

	(void)putchar('\n');
}

FILE *
file(char *name)
{
	FILE *ufp;

	if (!(ufp = fopen(name, "r"))) {
		err(1, "%s", name);
		/* NOTREACHED */
	}
	return(ufp);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: who [-HmqTu] [file]\n       who am i\n");
	exit(1);
}

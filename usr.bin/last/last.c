/*	$OpenBSD: last.c,v 1.9 1998/03/10 00:50:40 downsj Exp $	*/
/*	$NetBSD: last.c,v 1.6 1994/12/24 16:49:02 cgd Exp $	*/

/*
 * Copyright (c) 1987, 1993, 1994
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
static char copyright[] =
"@(#) Copyright (c) 1987, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)last.c	8.2 (Berkeley) 4/2/94";
#endif
static char rcsid[] = "$OpenBSD: last.c,v 1.9 1998/03/10 00:50:40 downsj Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tzfile.h>
#include <unistd.h>
#include <utmp.h>

#define	NO	0				/* false/no */
#define	YES	1				/* true/yes */
#define ATOI2(ar)       ((ar)[0] - '0') * 10 + ((ar)[1] - '0'); (ar) += 2;

static struct utmp	buf[1024];		/* utmp read buffer */

typedef struct arg {
	char	*name;				/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
	int	type;				/* type of arg */
	struct arg	*next;			/* linked list pointer */
} ARG;
ARG	*arglist;				/* head of linked list */

typedef struct ttytab {
	time_t	logout;				/* log out time */
	char	tty[UT_LINESIZE + 1];		/* terminal name */
	struct ttytab	*next;			/* linked list pointer */
} TTY;
TTY	*ttylist;				/* head of linked list */

static time_t	currentout;			/* current logout value */
static long	maxrec;				/* records to display */
static char	*file = _PATH_WTMP;		/* wtmp file */
static int	fulltime = 0;			/* Display seconds? */
static time_t	snaptime;			/* if != 0, we will only
						 * report users logged in
						 * at this snapshot time
						 */
static int calculate = 0;
static int seconds = 0;

void	 addarg __P((int, char *));
TTY	*addtty __P((char *));
void	 hostconv __P((char *));
void	 onintr __P((int));
char	*ttyconv __P((char *));
time_t	 dateconv __P((char *));
int	 want __P((struct utmp *, int));
void	 wtmp __P((void));
void 	 checkargs __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern int optind;
	extern char *optarg;
	int ch;
	char *p;

	maxrec = -1;
	snaptime = 0;
	while ((ch = getopt(argc, argv, "0123456789cf:h:st:d:T")) != -1)
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (maxrec == -1) {
				p = argv[optind - 1];
				if (p[0] == '-' && p[1] == ch && !p[2])
					maxrec = atol(++p);
				else
					maxrec = atol(argv[optind] + 1);
				if (!maxrec)
					exit(0);
			}
			break;
		case 'c':
			calculate++;
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 's':
			seconds++;
			break;
		case 't':
			addarg(TTY_TYPE, ttyconv(optarg));
			break;
		case 'd':
			snaptime = dateconv(optarg);
			break;
		case 'T':
			fulltime = 1;
			break;
		case '?':
		default:
			(void)fprintf(stderr,
			    "usage: last [-#] [-cns] [-f file] [-T] [-t tty]"
			    " [-h host] [-d [[yy]yy[mm[dd[hh]]]]mm[.ss]]"
			    " [user ...]\n");
			exit(1);
		}

	if (argc) {
		setlinebuf(stdout);
		for (argv += optind; *argv; ++argv) {
#define	COMPATIBILITY
#ifdef	COMPATIBILITY
			/* code to allow "last p5" to work */
			addarg(TTY_TYPE, ttyconv(*argv));
#endif
			addarg(USER_TYPE, *argv);
		}
	}
	
	checkargs();
	wtmp();
	exit(0);
}

/*
 * checkargs --
 * 	if snaptime is set, print warning if usernames, or -t or -h 
 *	flags are also provided
 */

void
checkargs()
{
   	ARG 	*step;
	int 	ttyflag = 0;

	if (!snaptime) 
		return;
	
	if (!arglist)
		return;

	for (step = arglist; step; step = step->next)
		switch (step->type) {
		case HOST_TYPE:
			(void)fprintf(stderr,
			    "Warning: Ignoring hostname flag\n");
			break;
		case TTY_TYPE:
			if (!ttyflag) { /* don't print this twice */ 
				(void)fprintf(stderr,
				    "Warning: Ignoring tty flag\n");
				ttyflag = 1;
			}
			break;
		case USER_TYPE:
			(void)fprintf(stderr,
			    "Warning: Ignoring username[s]\n");
			break;
		default:
			/* PRINT NOTHING */
		}
}      


/*
 * wtmp --
 *	read through the wtmp file
 */
void
wtmp()
{
	struct utmp	*bp;		/* current structure */
	TTY	*T;			/* tty list entry */
	struct stat	stb;		/* stat of file for size */
	time_t	delta;			/* time difference */
	time_t	total;
	off_t	bl;
	int	timesize;		/* how long time string gonna be */
	int	bytes, wfd;
	char	*ct, *crmsg;
	int 	snapfound = 0;		/* found snapshot entry? */
	if ((wfd = open(file, O_RDONLY, 0)) < 0 || fstat(wfd, &stb) == -1)
		err(1, "%s", file);
	bl = (stb.st_size + sizeof(buf) - 1) / sizeof(buf);

	if (fulltime)
		timesize = 8;	/* HH:MM:SS */
	else
		timesize = 5;	/* HH:MM */

	(void)time(&buf[0].ut_time);
	(void)signal(SIGINT, onintr);
	(void)signal(SIGQUIT, onintr);

	while (--bl >= 0) {
		if (lseek(wfd, bl * sizeof(buf), L_SET) == -1 ||
		    (bytes = read(wfd, buf, sizeof(buf))) == -1)
			err(1, "%s", file);
		for (bp = &buf[bytes / sizeof(buf[0]) - 1]; bp >= buf; --bp) {
			/*
			 * if the terminal line is '~', the machine stopped.
			 * see utmp(5) for more info.
			 */
			if (bp->ut_line[0] == '~' && !bp->ut_line[1]) {
				/* everybody just logged out */
				for (T = ttylist; T; T = T->next)
					T->logout = -bp->ut_time;
				currentout = -bp->ut_time;
				crmsg = strncmp(bp->ut_name, "shutdown",
				    UT_NAMESIZE) ? "crash" : "shutdown";
				/* 
				 * if we're in snapshop mode, we want to
				 * exit if this shutdown/reboot appears
				 * while we we are tracking the active
				 * range
				 */
				if (snaptime && snapfound)
					return;
				/* 
				 * don't print shutdown/reboot entries
				 * unless flagged for 
				 */ 
				if (want(bp, NO)) {
					if (seconds) {
				printf("%-*.*s %-*.*s %-*.*s %ld \n",
						UT_NAMESIZE, UT_NAMESIZE,
						bp->ut_name, UT_LINESIZE,
						UT_LINESIZE, bp->ut_line,
						UT_HOSTSIZE, UT_HOSTSIZE,
						bp->ut_host, bp->ut_time);
					} else {
						ct = ctime(&bp->ut_time);
				printf("%-*.*s  %-*.*s %-*.*s %10.10s %*.*s \n",
						UT_NAMESIZE, UT_NAMESIZE,
						bp->ut_name, UT_LINESIZE,
						UT_LINESIZE, bp->ut_line,
						UT_HOSTSIZE, UT_HOSTSIZE,
						bp->ut_host, ct, timesize,
						timesize, ct + 11);
					}
					if (maxrec != -1 && !--maxrec)
						return;
				}
				continue;
			}
			/*
			 * if the line is '{' or '|', date got set; see
			 * utmp(5) for more info.
			 */
			if ((bp->ut_line[0] == '{' || bp->ut_line[0] == '|')
			    && !bp->ut_line[1]) {
				if (want(bp, NO)) {
					if (seconds) {
				printf("%-*.*s %-*.*s %-*.*s %ld \n",
					UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					bp->ut_time);
				    	} else {
						ct = ctime(&bp->ut_time);
				printf("%-*.*s  %-*.*s %-*.*s %10.10s %*.*s \n",
					UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					ct, timesize, timesize, ct + 11);
				    	}
					if (maxrec && !--maxrec)
						return;
				}
				continue;
			}
			/* find associated tty */
			for (T = ttylist;; T = T->next) {
				if (!T) {
					/* add new one */
					T = addtty(bp->ut_line);
					break;
				}
				if (!strncmp(T->tty, bp->ut_line, UT_LINESIZE))
					break;
			}
			/* 
			 * print record if not in snapshot mode and wanted
			 * or in snapshot mode and in snapshot range
			 */
			if (bp->ut_name[0] &&
			    ((want(bp, YES)) ||
			     (bp->ut_time < snaptime && 
			      (T->logout > snaptime || !T->logout ||
			       T->logout < 0)))) {
				snapfound = 1;
				if (seconds) {
				printf("%-*.*s %-*.*s %-*.*s %ld ",
					UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					bp->ut_time);
				} else {
					ct = ctime(&bp->ut_time);
				printf("%-*.*s  %-*.*s %-*.*s %10.10s %*.*s ",
					UT_NAMESIZE, UT_NAMESIZE, bp->ut_name,
					UT_LINESIZE, UT_LINESIZE, bp->ut_line,
					UT_HOSTSIZE, UT_HOSTSIZE, bp->ut_host,
					ct, timesize, timesize, ct + 11);
				}
				if (!T->logout)
					puts("  still logged in");
				else {
					if (T->logout < 0) {
						T->logout = -T->logout;
						printf("- %s", crmsg);
					} else {
						if (seconds) 
							printf("- %ld",
							    T->logout);
						else
							printf("- %*.*s",
							    timesize, timesize,
							    ctime(&T->logout)+11);
					}
					delta = T->logout - bp->ut_time;
					if (seconds)
						printf("  (%ld)\n", delta);
					else {
						if (delta < SECSPERDAY)
							printf("  (%*.*s)\n",
							    timesize, timesize,
							    asctime(gmtime(&delta))+11);
						else
							printf(" (%ld+%*.*s)\n",
							    delta / SECSPERDAY,
							    timesize, timesize,
							    asctime(gmtime(&delta))+11);
					}
					if (calculate)
						total += delta;
				}
				if (maxrec != -1 && !--maxrec)
					return;
			}
			T->logout = bp->ut_time;
		}
	}
	if (calculate) {
		if ((total / SECSPERDAY) > 0) {
			int days = (total / SECSPERDAY);
			total -= (days * SECSPERDAY);

			printf("\nTotal time: %d days, %*.*s\n",
				days, timesize, timesize,
				asctime(gmtime(&total))+11);
		} else
			printf("\nTotal time: %*.*s\n",
				timesize, timesize,
				asctime(gmtime(&total))+11);
	}
	ct = ctime(&buf[0].ut_time);
	printf("\nwtmp begins %10.10s %*.*s \n", ct, timesize, timesize, ct + 11);
}

/*
 * want --
 *	see if want this entry
 */
int
want(bp, check)
	struct utmp *bp;
	int check;
{
	ARG *step;

	if (check)
		/*
		 * when uucp and ftp log in over a network, the entry in
		 * the utmp file is the name plus their process id.  See
		 * etc/ftpd.c and usr.bin/uucp/uucpd.c for more information.
		 */
		if (!strncmp(bp->ut_line, "ftp", sizeof("ftp") - 1))
			bp->ut_line[3] = '\0';
		else if (!strncmp(bp->ut_line, "uucp", sizeof("uucp") - 1))
			bp->ut_line[4] = '\0';

	if (snaptime) 		/* if snaptime is set, return NO */
		return (NO);

	if (!arglist)
		return (YES);

	for (step = arglist; step; step = step->next)
		switch(step->type) {
		case HOST_TYPE:
			if (!strncasecmp(step->name, bp->ut_host, UT_HOSTSIZE))
				return (YES);
			break;
		case TTY_TYPE:
			if (!strncmp(step->name, bp->ut_line, UT_LINESIZE))
				return (YES);
			break;
		case USER_TYPE:
			if (!strncmp(step->name, bp->ut_name, UT_NAMESIZE))
				return (YES);
			break;
		}

	return (NO);
}

/*
 * addarg --
 *	add an entry to a linked list of arguments
 */
void
addarg(type, arg)
	int type;
	char *arg;
{
	ARG *cur;

	if (!(cur = (ARG *)malloc((u_int)sizeof(ARG))))
		err(1, "malloc failure");
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * addtty --
 *	add an entry to a linked list of ttys
 */
TTY *
addtty(ttyname)
	char *ttyname;
{
	TTY *cur;

	if (!(cur = (TTY *)malloc((u_int)sizeof(TTY))))
		err(1, "malloc failure");
	cur->next = ttylist;
	cur->logout = currentout;
	memmove(cur->tty, ttyname, UT_LINESIZE);
	return (ttylist = cur);
}

/*
 * hostconv --
 *	convert the hostname to search pattern; if the supplied host name
 *	has a domain attached that is the same as the current domain, rip
 *	off the domain suffix since that's what login(1) does.
 */
void
hostconv(arg)
	char *arg;
{
	static int first = 1;
	static char *hostdot, name[MAXHOSTNAMELEN];
	char *argdot;

	if (!(argdot = strchr(arg, '.')))
		return;
	if (first) {
		first = 0;
		if (gethostname(name, sizeof(name)))
			err(1, "gethostname");
		hostdot = strchr(name, '.');
	}
	if (hostdot && !strcasecmp(hostdot, argdot))
		*argdot = '\0';
}

/*
 * ttyconv --
 *	convert tty to correct name.
 */
char *
ttyconv(arg)
	char *arg;
{
	char *mval;

	/*
	 * kludge -- we assume that all tty's end with
	 * a two character suffix.
	 */
	if (strlen(arg) == 2) {
		/* either 6 for "ttyxx" or 8 for "console" */
		if (!(mval = malloc((u_int)8)))
			err(1, "malloc failure");
		if (!strcmp(arg, "co"))
			(void)strcpy(mval, "console");
		else {
			(void)strcpy(mval, "tty");
			(void)strcpy(mval + 3, arg);
		}
		return (mval);
	}
	if (!strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1))
		return (arg + 5);
	return (arg);
}

/* 
 * dateconv --
 * 	Convert the snapshot time in command line given in the format
 * 	[[yy]yy][mmdd]hhmm[.ss]] to a time_t.
 * 	Derived from atime_arg1() in usr.bin/touch/touch.c
 */
time_t
dateconv(arg)
	char *arg;
{
	time_t timet;
	struct tm *t;
	int yearset;
	char *p;

	/* Start with the current time. */
	if (time(&timet) < 0)
		err(1, "time");
	if ((t = localtime(&timet)) == NULL)
		err(1, "localtime");

	/* [[yy]yy][mmdd]hhmm[.ss] */
	if ((p = strchr(arg, '.')) == NULL)
		t->tm_sec = 0; 		/* Seconds defaults to 0. */
	else {
		if (strlen(p + 1) != 2)
			goto terr;
		*p++ = '\0';
		t->tm_sec = ATOI2(p);
	}

	yearset = 0;
	switch (strlen(arg)) {
	case 12:			/* ccyymmddhhmm */
		t->tm_year = ATOI2(arg);
		t->tm_year *= 100;
		yearset = 1;
		/* FALLTHOUGH */
	case 10:			/* yymmddhhmm */
		if (yearset) {
			yearset = ATOI2(arg);
			t->tm_year += yearset;
		} else {
			yearset = ATOI2(arg);
			if (yearset < 69)
				t->tm_year = yearset + 2000;
			else
				t->tm_year = yearset + 1900;
		}
		t->tm_year -= 1900;     /* Convert to UNIX time. */
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		t->tm_mon = ATOI2(arg);
		--t->tm_mon;    	/* Convert from 01-12 to 00-11 */
		t->tm_mday = ATOI2(arg);
		t->tm_hour = ATOI2(arg);
		t->tm_min = ATOI2(arg);
		break;
	case 4:				/* hhmm */
		t->tm_hour = ATOI2(arg);
		t->tm_min = ATOI2(arg);
		break;
	default:
		goto terr;
	}
	t->tm_isdst = -1;       	/* Figure out DST. */
	timet = mktime(t);
	if (timet == -1)
terr:	   errx(1,
	"out of range or illegal time specification: [[yy]yy][mmdd]hhmm[.ss]");
	return timet;
}


/*
 * onintr --
 *	on interrupt, we inform the user how far we've gotten
 */
void
onintr(signo)
	int signo;
{
	char *ct;

	ct = ctime(&buf[0].ut_time);
	printf("\ninterrupted %10.10s %8.8s \n", ct, ct + 11);
	if (signo == SIGINT)
		exit(1);
	(void)fflush(stdout);			/* fix required for rsh */
}

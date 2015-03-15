/*	$OpenBSD: last.c,v 1.48 2015/03/15 00:41:28 millert Exp $	*/
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

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <utmp.h>

#define	NO	0				/* false/no */
#define	YES	1				/* true/yes */
#define ATOI2(ar)	((ar)[0] - '0') * 10 + ((ar)[1] - '0'); (ar) += 2;

static struct utmp	buf[1024];		/* utmp read buffer */

struct arg {
	char	*name;				/* argument */
#define	HOST_TYPE	-2
#define	TTY_TYPE	-3
#define	USER_TYPE	-4
	int	type;				/* type of arg */
	struct	arg *next;			/* linked list pointer */
} *arglist;

struct ttytab {
	time_t	logout;				/* log out time */
	char	tty[UT_LINESIZE + 1];		/* terminal name */
	struct	ttytab*next;			/* linked list pointer */
} *ttylist;

static time_t	currentout;			/* current logout value */
static long	maxrec = -1;			/* records to display */
static char	*file = _PATH_WTMP;		/* wtmp file */
static int	fulltime = 0;			/* Display seconds? */
static time_t	snaptime = 0;			/* report only at this time */
static int	calculate = 0;
static int	seconds = 0;

void	 addarg(int, char *);
struct ttytab	*addtty(char *);
void	 hostconv(char *);
void	 onintr(int);
char	*ttyconv(char *);
time_t	 dateconv(char *);
int	 want(struct utmp *, int);
void	 wtmp(void);
void	 checkargs(void);
void	 print_entry(const struct utmp *);
void	 usage(void);

#define NAME_WIDTH	9
#define HOST_WIDTH	24

#define SECSPERDAY	(24 * 60 * 60)

int
main(int argc, char *argv[])
{
	const char *errstr;
	int ch, lastch = '\0', newarg = 1, prevoptind = 1;

	while ((ch = getopt(argc, argv, "0123456789cf:h:n:st:d:T")) != -1) {
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			/*
			 * kludge: last was originally designed to take
			 * a number after a dash.
			 */
			if (newarg || !isdigit(lastch))
				maxrec = 0;
			else if (maxrec > INT_MAX / 10)
				usage();
			maxrec = (maxrec * 10) + (ch - '0');
			break;
		case 'c':
			calculate = 1;
			break;
		case 'f':
			file = optarg;
			break;
		case 'h':
			hostconv(optarg);
			addarg(HOST_TYPE, optarg);
			break;
		case 'n':
			maxrec = strtonum(optarg, 0, LONG_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "number of lines is %s: %s", errstr,
				    optarg);
			if (maxrec == 0)
				exit(0);
			break;
		case 's':
			seconds = 1;
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
		default:
			usage();
		}
		lastch = ch;
		newarg = optind != prevoptind;
		prevoptind = optind;
	}
	if (maxrec == 0)
		exit(0);

	if (argc) {
		setvbuf(stdout, NULL, _IOLBF, 0);
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
 * if snaptime is set, print warning if usernames, or -t or -h
 * flags are also provided
 */
void
checkargs(void)
{
	int	ttyflag = 0;
	struct arg *step;

	if (!snaptime || !arglist)
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
			break;
			/* PRINT NOTHING */
		}
}

void
print_entry(const struct utmp *bp)
{
	printf("%-*.*s %-*.*s %-*.*s ",
	    NAME_WIDTH, UT_NAMESIZE, bp->ut_name,
	    UT_LINESIZE, UT_LINESIZE, bp->ut_line,
	    HOST_WIDTH, UT_HOSTSIZE, bp->ut_host);

	if (seconds)
		printf("%lld", (long long)bp->ut_time);
	else {
		struct tm *tm;

		tm = localtime(&bp->ut_time);
		if (tm == NULL) {
			/* bogus entry?  format as epoch time... */
			printf("%lld", (long long)bp->ut_time);
		} else {
			char	tim[40];

			strftime(tim, sizeof tim,
			    fulltime ? "%a %b %d %H:%M:%S" : "%a %b %d %H:%M",
			    tm);
			printf("%s", tim);
		}
	}
}


/*
 * read through the wtmp file
 */
void
wtmp(void)
{
	time_t	delta, total = 0;
	int	timesize, wfd, snapfound = 0;
	char	*ct, *crmsg = "invalid";
	struct utmp	*bp;
	struct stat	stb;
	ssize_t	bytes;
	off_t	bl;
	struct ttytab	*T;

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
		if (lseek(wfd, bl * sizeof(buf), SEEK_SET) == -1 ||
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
				 * if we're in snapshot mode, we want to
				 * exit if this shutdown/reboot appears
				 * while we we are tracking the active
				 * range
				 */
				if (snaptime && snapfound) {
					close(wfd);
					return;
				}

				/*
				 * don't print shutdown/reboot entries
				 * unless flagged for
				 */
				if (want(bp, NO)) {
					print_entry(bp);
					printf("\n");
					if (maxrec != -1 && !--maxrec) {
						close(wfd);
						return;
					}
				}
				continue;
			}

			/*
			 * if the line is '{' or '|', date got set; see
			 * utmp(5) for more info.
			 */
			if ((bp->ut_line[0] == '{' || bp->ut_line[0] == '|') &&
			    !bp->ut_line[1]) {
				if (want(bp, NO)) {
					print_entry(bp);
					printf("\n");
					if (maxrec && !--maxrec) {
						close(wfd);
						return;
					}
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
			    ((want(bp, YES)) || (bp->ut_time < snaptime &&
			    (T->logout > snaptime || !T->logout ||
			    T->logout < 0)))) {
				snapfound = 1;
				print_entry(bp);
				printf(" ");

				if (!T->logout)
					puts("  still logged in");
				else {
					if (T->logout < 0) {
						T->logout = -T->logout;
						printf("- %s", crmsg);
					} else {
						if (seconds)
							printf("- %lld",
							    (long long)T->logout);
						else
							printf("- %*.*s",
							    timesize, timesize,
							    ctime(&T->logout)+11);
					}
					delta = T->logout - bp->ut_time;
					if (seconds)
						printf("  (%lld)\n",
						    (long long)delta);
					else {
						if (delta < SECSPERDAY)
							printf("  (%*.*s)\n",
							    timesize, timesize,
							    asctime(gmtime(&delta))+11);
						else
							printf(" (%lld+%*.*s)\n",
							    (long long)delta / SECSPERDAY,
							    timesize, timesize,
							    asctime(gmtime(&delta))+11);
					}
					if (calculate)
						total += delta;
				}
				if (maxrec != -1 && !--maxrec) {
					close(wfd);
					return;
				}
			}
			T->logout = bp->ut_time;
		}
	}
	close(wfd);
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
	printf("\n%s begins %10.10s %*.*s %4.4s\n", basename(file), ct,
	    timesize, timesize, ct + 11, ct + 20);
}

/*
 * see if want this entry
 */
int
want(struct utmp *bp, int check)
{
	struct arg *step;

	if (check) {
		/*
		 * some entries, such as ftp and uucp, will 
		 * include process name plus id; exclude entries
		 * that start with 'console' and 'tty' from
		 * having the process id stripped.
		 */
		if ((strncmp(bp->ut_line, "console", strlen("console")) != 0) &&
		    (strncmp(bp->ut_line, "tty", strlen("tty")) != 0)) {
			char *s;
			for (s = bp->ut_line;
			     *s != '\0' && !isdigit((unsigned char)*s); s++)
				;
			*s = '\0';
		}
	}

	if (snaptime)		/* if snaptime is set, return NO */
		return (NO);

	if (!arglist)
		return (YES);

	for (step = arglist; step; step = step->next)
		switch (step->type) {
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
 * add an entry to a linked list of arguments
 */
void
addarg(int type, char *arg)
{
	struct arg *cur;

	if (!(cur = (struct arg *)malloc((u_int)sizeof(struct arg))))
		err(1, "malloc failure");
	cur->next = arglist;
	cur->type = type;
	cur->name = arg;
	arglist = cur;
}

/*
 * add an entry to a linked list of ttys
 */
struct ttytab *
addtty(char *ttyname)
{
	struct ttytab *cur;

	if (!(cur = (struct ttytab *)malloc((u_int)sizeof(struct ttytab))))
		err(1, "malloc failure");
	cur->next = ttylist;
	cur->logout = currentout;
	memmove(cur->tty, ttyname, UT_LINESIZE);
	return (ttylist = cur);
}

/*
 * convert the hostname to search pattern; if the supplied host name
 * has a domain attached that is the same as the current domain, rip
 * off the domain suffix since that's what login(1) does.
 */
void
hostconv(char *arg)
{
	static char *hostdot, name[HOST_NAME_MAX+1];
	static int first = 1;
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
 * convert tty to correct name.
 */
char *
ttyconv(char *arg)
{
	size_t len = 8;
	char *mval;

	/*
	 * kludge -- we assume that all tty's end with
	 * a two character suffix.
	 */
	if (strlen(arg) == 2) {
		/* either 6 for "ttyxx" or 8 for "console" */
		if (!(mval = malloc(len)))
			err(1, "malloc failure");
		if (!strcmp(arg, "co"))
			(void)strlcpy(mval, "console", len);
		else
			snprintf(mval, len, "tty%s", arg);
		return (mval);
	}
	if (!strncmp(arg, _PATH_DEV, sizeof(_PATH_DEV) - 1))
		return (arg + 5);
	return (arg);
}

/*
 * Convert the snapshot time in command line given in the format
 *	[[[CC]YY]MMDD]hhmm[.SS]] to a time_t.
 *	Derived from atime_arg1() in usr.bin/touch/touch.c
 */
time_t
dateconv(char *arg)
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

	/* [[[CC]YY]MMDD]hhmm[.SS] */
	if ((p = strchr(arg, '.')) == NULL)
		t->tm_sec = 0;		/* Seconds defaults to 0. */
	else {
		if (strlen(p + 1) != 2)
			goto terr;
		*p++ = '\0';
		t->tm_sec = ATOI2(p);
	}

	yearset = 0;
	switch (strlen(arg)) {
	case 12:			/* CCYYMMDDhhmm */
		t->tm_year = ATOI2(arg);
		t->tm_year *= 100;
		yearset = 1;
		/* FALLTHROUGH */
	case 10:			/* YYMMDDhhmm */
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
		t->tm_year -= 1900;	/* Convert to UNIX time. */
		/* FALLTHROUGH */
	case 8:				/* MMDDhhmm */
		t->tm_mon = ATOI2(arg);
		--t->tm_mon;		/* Convert from 01-12 to 00-11 */
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
	t->tm_isdst = -1;		/* Figure out DST. */
	timet = mktime(t);
	if (timet == -1)
terr:		errx(1, "out of range or illegal time specification: "
		    "[[[CC]YY]MMDD]hhmm[.SS]");
	return (timet);
}


/*
 * on interrupt, we inform the user how far we've gotten
 */
void
onintr(int signo)
{
	char str[1024], *ct, ctbuf[26];

	ct = ctime_r(&buf[0].ut_time, ctbuf);
	snprintf(str, sizeof str, "\ninterrupted %10.10s %8.8s \n",
	    ct, ct + 11);
	write(STDOUT_FILENO, str, strlen(str));
	if (signo == SIGINT)
		_exit(1);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-csT] [-d date] [-f file] [-h host]"
	    " [-n number] [-t tty] [user ...]\n", __progname);
	exit(1);
}

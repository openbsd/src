/*	$OpenBSD: main.c,v 1.37 2007/05/21 21:15:37 cnst Exp $	*/
/*	$NetBSD: main.c,v 1.8 1996/05/10 23:16:36 thorpej Exp $	*/

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
static char copyright[] =
"@(#) Copyright (c) 1980, 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static char rcsid[] = "$OpenBSD: main.c,v 1.37 2007/05/21 21:15:37 cnst Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/sysctl.h>

#include <err.h>
#include <nlist.h>
#include <signal.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

#include "systat.h"
#include "extern.h"

double	dellave;

kvm_t	*kd;
char	*nlistf = NULL;
char	*memf = NULL;
double	avenrun[3];
double	naptime = 5.0;
int	verbose = 1;		/* to report kvm read errs */
int	nflag = 0;
int	ut, hz, stathz;
char    hostname[MAXHOSTNAMELEN];
WINDOW  *wnd;
int	CMDLINE;

WINDOW *wload;			/* one line window for load average */

static void usage(void);

int
main(int argc, char *argv[])
{
	char errbuf[_POSIX2_LINE_MAX];
	gid_t gid;
	int ch;

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0) {
		error("No utmp");
		exit(1);
	}

	kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (kd == NULL) {
		error("%s", errbuf);
		exit(1);
	}

	gid = getgid();
	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid");

	while ((ch = getopt(argc, argv, "nw:")) != -1)
		switch (ch) {
		case 'n':
			nflag = 1;
			break;
		case 'w':

			naptime = strtod(optarg, NULL);
			if (naptime < 0.09 || naptime > 1000.0)
				errx(1, "invalid interval: %s", optarg);
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	while (argc > 0) {
		if (isdigit(argv[0][0])) {
			naptime = strtod(argv[0], NULL);
			if (naptime < 0.09 || naptime > 1000.0)
				naptime = 5.0;
		} else {
			struct cmdtab *p;

			p = lookup(&argv[0][0]);
			if (p == (struct cmdtab *)-1)
				errx(1, "ambiguous request: %s", &argv[0][0]);
			if (p == 0)
				errx(1, "unknown request: %s", &argv[0][0]);
			curcmd = p;
		}
		argc--;
		argv++;
	}

	signal(SIGINT, sigdie);
	siginterrupt(SIGINT, 1);
	signal(SIGQUIT, sigdie);
	siginterrupt(SIGQUIT, 1);
	signal(SIGTERM, sigdie);
	siginterrupt(SIGTERM, 1);
	signal(SIGTSTP, sigtstp);
	siginterrupt(SIGTSTP, 1);

	/*
	 * Initialize display.  Load average appears in a one line
	 * window of its own.  Current command's display appears in
	 * an overlapping sub-window of stdscr configured by the display
	 * routines to minimize update work by curses.
	 */
	if (initscr() == NULL) {
		warnx("couldn't initialize screen");
		exit(0);
	}

	CMDLINE = LINES - 1;
	wnd = (*curcmd->c_open)();
	if (wnd == NULL) {
		warnx("couldn't initialize display");
		die();
	}
	wload = newwin(1, 0, 1, 20);
	if (wload == NULL) {
		warnx("couldn't set up load average window");
		die();
	}
	gethostname(hostname, sizeof (hostname));
	gethz();
	(*curcmd->c_init)();
	curcmd->c_flags |= CF_INIT;
	labels();

	dellave = 0.0;

	signal(SIGALRM, sigdisplay);
	siginterrupt(SIGALRM, 1);
	signal(SIGWINCH, sigwinch);
	siginterrupt(SIGWINCH, 1);
	gotdisplay = 1;
	noecho();
	crmode();
	keyboard();
	/*NOTREACHED*/
}

void
gethz(void)
{
	struct clockinfo cinf;
	size_t  size = sizeof(cinf);
	int	mib[2];

	mib[0] = CTL_KERN;
	mib[1] = KERN_CLOCKRATE;
	if (sysctl(mib, 2, &cinf, &size, NULL, 0) == -1)
		return;
	stathz = cinf.stathz;
	hz = cinf.hz;
}

static void
usage(void)
{
	fprintf(stderr, "usage: systat [-n] [-w wait] [display] [refresh-interval]\n");
	exit(1);
}


void
labels(void)
{
	if (curcmd->c_flags & CF_LOADAV)
		mvprintw(0, 2 + 4, "users    Load");
	(*curcmd->c_label)();
#ifdef notdef
	mvprintw(21, 25, "CPU usage on %s", hostname);
#endif
	refresh();
}

/*ARGSUSED*/
void
sigdisplay(int signo)
{
	gotdisplay = 1;
}

void
display(void)
{
	/* Get the load average over the last minute. */
	(void) getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));
	(*curcmd->c_fetch)();
	if (curcmd->c_flags & CF_LOADAV) {
		extern int ucount();
		char tbuf[26];
		time_t now;

		time(&now);
		strlcpy(tbuf, ctime(&now), sizeof tbuf);

		putint(ucount(), 0, 2, 3);
		putfloat(avenrun[0], 0, 2 + 17, 6, 2, 0);
		putfloat(avenrun[1], 0, 2 + 23, 6, 2, 0);
		putfloat(avenrun[2], 0, 2 + 29, 6, 2, 0);
		mvaddstr(0, 2 + 53, tbuf);
	}
	(*curcmd->c_refresh)();
	if (curcmd->c_flags & CF_LOADAV)
		wrefresh(wload);
	wrefresh(wnd);
	move(CMDLINE, 0);
	refresh();
	ualarm(naptime * 1000000, 0);
}

void
load(void)
{

	(void) getloadavg(avenrun, sizeof(avenrun)/sizeof(avenrun[0]));
	mvprintw(CMDLINE, 0, "%4.1f %4.1f %4.1f",
	    avenrun[0], avenrun[1], avenrun[2]);
	clrtoeol();
}

volatile sig_atomic_t gotdie;
volatile sig_atomic_t gotdisplay;
volatile sig_atomic_t gotwinch;
volatile sig_atomic_t gottstp;

/*ARGSUSED*/
void
sigdie(int signo)
{
	gotdie = 1;
}

/*ARGSUSED*/
void
sigtstp(int signo)
{
	gottstp = 1;
}

void
die(void)
{
	if (wnd) {
		move(CMDLINE, 0);
		clrtoeol();
		refresh();
		endwin();
	}
	exit(0);
}

/*ARGSUSED*/
void
sigwinch(int signo)
{
	gotwinch = 1;
}

void
error(const char *fmt, ...)
{
	va_list ap;
	char buf[255];
	int oy, ox;

	va_start(ap, fmt);
	if (wnd) {
		getyx(stdscr, oy, ox);
		(void) vsnprintf(buf, sizeof buf, fmt, ap);
		clrtoeol();
		standout();
		mvaddstr(CMDLINE, 0, buf);
		standend();
		move(oy, ox);
		refresh();
	} else {
		(void) vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
	va_end(ap);
}

void
nlisterr(struct nlist namelist[])
{
	int i, n;

	n = 0;
	clear();
	mvprintw(2, 10, "systat: nlist: can't find following symbols:");
	for (i = 0;
	    namelist[i].n_name != NULL && *namelist[i].n_name != '\0'; i++)
		if (namelist[i].n_value == 0)
			mvprintw(2 + ++n, 10, "%s", namelist[i].n_name);
	move(CMDLINE, 0);
	clrtoeol();
	refresh();
	endwin();
	exit(1);
}

/* calculate number of users on the system */
int
ucount(void)
{
	int nusers = 0;
	struct	utmp utmp;

	if (ut < 0)
		return (0);
	lseek(ut, (off_t)0, SEEK_SET);
	while (read(ut, &utmp, sizeof(utmp)))
		if (utmp.ut_name[0] != '\0')
			nusers++;

	return (nusers);
}

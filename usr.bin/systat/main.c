/*	$OpenBSD: main.c,v 1.29 2004/04/14 19:53:04 deraadt Exp $	*/
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
static char rcsid[] = "$OpenBSD: main.c,v 1.29 2004/04/14 19:53:04 deraadt Exp $";
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
#include <stdlib.h>
#include <limits.h>
#include <stdarg.h>

#include "systat.h"
#include "extern.h"

double	dellave;

kvm_t	*kd;
char	*nlistf = NULL;
char	*memf = NULL;
sig_t	sigtstpdfl;
double	avenrun[3];
int	col;
int	naptime = 5;
int	verbose = 1;		/* to report kvm read errs */
int	hz, stathz;
char    c;
char    *namp;
char    hostname[MAXHOSTNAMELEN];
WINDOW  *wnd;
long	CMDLINE;

WINDOW *wload;			/* one line window for load average */

static void usage(void);

int
main(int argc, char *argv[])
{
	int ch;
	char errbuf[_POSIX2_LINE_MAX];

	kd = kvm_openfiles(NULL, NULL, NULL, O_RDONLY, errbuf);
	if (kd == NULL) {
		error("%s", errbuf);
		exit(1);
	}

	setegid(getgid());
	setgid(getgid());

	while ((ch = getopt(argc, argv, "w:")) != -1)
		switch (ch) {
		case 'w':
			if ((naptime = atoi(optarg)) <= 0)
				errx(1, "interval <= 0.");
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	while (argc > 0) {
		if (isdigit(argv[0][0])) {
			naptime = atoi(argv[0]);
			if (naptime <= 0)
				naptime = 5;
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
	wload = newwin(1, 0, 3, 20);
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
	fprintf(stderr, "usage: systat [-w wait] [display] [refresh-interval]\n");
	exit(1);
}


void
labels(void)
{
	if (curcmd->c_flags & CF_LOADAV) {
		mvaddstr(2, 20,
		    "/0   /1   /2   /3   /4   /5   /6   /7   /8   /9   /10");
		mvaddstr(3, 5, "Load Average");
	}
	(*curcmd->c_label)();
#ifdef notdef
	mvprintw(21, 25, "CPU usage on %s", hostname);
#endif
	refresh();
}

void
sigdisplay(signo)
	int signo;
{
	gotdisplay = 1;
}

void
display(void)
{
	int i, j;

	/* Get the load average over the last minute. */
	(void) getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));
	(*curcmd->c_fetch)();
	if (curcmd->c_flags & CF_LOADAV) {
		j = 5.0*avenrun[0] + 0.5;
		dellave -= avenrun[0];
		if (dellave >= 0.0)
			c = '<';
		else {
			c = '>';
			dellave = -dellave;
		}
		if (dellave < 0.05)
			c = '|';
		dellave = avenrun[0];
		wmove(wload, 0, 0);
		wclrtoeol(wload);
		for (i = (j > 50) ? 50 : j; i > 0; i--)
			waddch(wload, c);
		if (j > 50)
			wprintw(wload, " %4.1f", avenrun[0]);
	}
	(*curcmd->c_refresh)();
	if (curcmd->c_flags & CF_LOADAV)
		wrefresh(wload);
	wrefresh(wnd);
	move(CMDLINE, col);
	refresh();
	alarm(naptime);
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

void
sigdie(int signo)
{
	gotdie = 1;
}

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

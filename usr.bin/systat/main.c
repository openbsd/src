/* $Id: main.c,v 1.68 2018/05/30 13:43:51 krw Exp $	 */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar
 * Copyright (c) 2001 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/types.h>
#include <sys/sysctl.h>


#include <ctype.h>
#include <curses.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <utmp.h>

#include "engine.h"
#include "systat.h"

#define TIMEPOS (80 - 8 - 20 - 1)

double	dellave;

kvm_t	*kd;
char	*nlistf = NULL;
char	*memf = NULL;
double	avenrun[3];
double	naptime = 5.0;
int	verbose = 1;		/* to report kvm read errs */
int	nflag = 1;
int	ut, hz, stathz;
char    hostname[HOST_NAME_MAX+1];
WINDOW  *wnd;
int	CMDLINE;
char	timebuf[26];
char	uloadbuf[TIMEPOS];


int  ucount(void);
void usage(void);

/* command prompt */

void cmd_delay(const char *);
void cmd_count(const char *);
void cmd_compat(const char *);

struct command cm_compat = {"Command", cmd_compat};
struct command cm_delay = {"Seconds to delay", cmd_delay};
struct command cm_count = {"Number of lines to display", cmd_count};


/* display functions */

int
print_header(void)
{
	time_t now;
	int start = dispstart + 1, end = dispstart + maxprint;
	char tmpbuf[TIMEPOS];
	char header[MAX_LINE_BUF];

	if (end > num_disp)
		end = num_disp;

	tb_start();

	if (!paused) {
		char *ctim;

		getloadavg(avenrun, sizeof(avenrun) / sizeof(avenrun[0]));

		snprintf(uloadbuf, sizeof(uloadbuf),
		    "%4d users Load %.2f %.2f %.2f", 
		    ucount(), avenrun[0], avenrun[1], avenrun[2]);

		time(&now);
		ctim = ctime(&now);
		ctim[11+8] = '\0';
		strlcpy(timebuf, ctim + 11, sizeof(timebuf));
	}

	if (num_disp && (start > 1 || end != num_disp))
		snprintf(tmpbuf, sizeof(tmpbuf),
		    "%s (%u-%u of %u) %s", uloadbuf, start, end, num_disp,
		    paused ? "PAUSED" : "");
	else
		snprintf(tmpbuf, sizeof(tmpbuf), 
		    "%s %s", uloadbuf,
		    paused ? "PAUSED" : "");
		
	snprintf(header, sizeof(header), "%-*s %19.19s %s", TIMEPOS - 1,
	    tmpbuf, hostname, timebuf);

	if (rawmode)
		printf("\n\n%s\n", header);
	else
		mvprintw(0, 0, "%s", header);

	return (1);
}

/* compatibility functions, rearrange later */
void
error(const char *fmt, ...)
{
	va_list ap;
	char buf[MAX_LINE_BUF];

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	message_set(buf);
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

void
die(void)
{
	if (!rawmode)
		endwin();
	exit(0);
}


int
prefix(char *s1, char *s2)
{

	while (*s1 == *s2) {
		if (*s1 == '\0')
			return (1);
		s1++, s2++;
	}
	return (*s1 == '\0');
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

/* main program functions */

void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-aBbiNn] [-d count] "
	    "[-s delay] [-w width] [view] [delay]\n", __progname);
	exit(1);
}

void
show_view(void)
{
	if (rawmode)
		return;

	tb_start();
	tbprintf("%s %g", curr_view->name, naptime);
	tb_end();
	message_set(tmp_buf);
}

void
add_view_tb(field_view *v)
{
	if (curr_view == v)
		tbprintf("[%s] ", v->name);
	else
		tbprintf("%s ", v->name);
}

void
show_help(void)
{
	if (rawmode)
		return;

	tb_start();
	foreach_view(add_view_tb);
	tb_end();
	message_set(tmp_buf);
}

void
add_order_tb(order_type *o)
{
	if (curr_view->mgr->order_curr == o)
		tbprintf("[%s%s(%c)] ", o->name,
		    o->func != NULL && sortdir == -1 ? "^" : "",
		    (char) o->hotkey);
	else
		tbprintf("%s(%c) ", o->name, (char) o->hotkey);
}

void
show_order(void)
{
	if (rawmode)
		return;

	tb_start();
	if (foreach_order(add_order_tb) == -1) {
		tbprintf("No orders available");
	}
	tb_end();
	message_set(tmp_buf);
}

void
cmd_compat(const char *buf)
{
	const char *s;

	if (strcasecmp(buf, "help") == 0) {
		show_help();
		need_update = 1;
		return;
	}
	if (strcasecmp(buf, "quit") == 0 || strcasecmp(buf, "q") == 0) {
		gotsig_close = 1;
		return;
	}
	if (strcasecmp(buf, "stop") == 0) {
		paused = 1;
		gotsig_alarm = 1;
		return;
	}
	if (strncasecmp(buf, "start", 5) == 0) {
		paused = 0;
		gotsig_alarm = 1;
		cmd_delay(buf + 5);
		return;
	}
	if (strncasecmp(buf, "order", 5) == 0) {
		show_order();
		need_update = 1;
		return;
	}

	for (s = buf; *s && strchr("0123456789+-.eE", *s) != NULL; s++)
		;
	if (*s) {
		if (set_view(buf))
			error("Invalid/ambiguous view: %s", buf);
	} else
		cmd_delay(buf);
}

void
cmd_delay(const char *buf)
{
	double del;
	del = atof(buf);

	if (del > 0) {
		udelay = (useconds_t)(del * 1000000);
		gotsig_alarm = 1;
		naptime = del;
	}
}

void
cmd_count(const char *buf)
{
	const char *errstr;

	maxprint = strtonum(buf, 1, lines - HEADER_LINES, &errstr);
	if (errstr)
		maxprint = lines - HEADER_LINES;
}


int
keyboard_callback(int ch)
{
	switch (ch) {
	case '?':
		/* FALLTHROUGH */
	case 'h':
		show_help();
		need_update = 1;
		break;
	case CTRL_G:
		show_view();
		need_update = 1;
		break;
	case 'l':
		command_set(&cm_count, NULL);
		break;
	case 's':
		command_set(&cm_delay, NULL);
		break;
	case ',':
		separate_thousands = !separate_thousands;
		gotsig_alarm = 1;
		break;
	case ':':
		command_set(&cm_compat, NULL);
		break;
	default:
		return 0;
	};

	return 1;
}

void
initialize(void)
{
	engine_initialize();

	initvmstat();
	initpigs();
	initifstat();
	initiostat();
	initsensors();
	initmembufs();
	initnetstat();
	initswap();
	initpftop();
	initpf();
	initpool();
	initmalloc();
	initnfs();
	initcpu();
	inituvm();
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

int
main(int argc, char *argv[])
{
	char errbuf[_POSIX2_LINE_MAX];
	const char *errstr;
	extern char *optarg;
	extern int optind;
	double delay = 5;

	char *viewstr = NULL;

	gid_t gid;
	int countmax = 0;
	int maxlines = 0;

	int ch;

	ut = open(_PATH_UTMP, O_RDONLY);
	if (ut < 0) {
		warn("No utmp");
	}

	kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);

	gid = getgid();
	if (setresgid(gid, gid, gid) == -1)
		err(1, "setresgid");

	while ((ch = getopt(argc, argv, "BNabd:ins:w:")) != -1) {
		switch (ch) {
		case 'a':
			maxlines = -1;
			break;
		case 'B':
			averageonly = 1;
			if (countmax < 2)
				countmax = 2;
			/* FALLTHROUGH */
		case 'b':
			rawmode = 1;
			interactive = 0;
			break;
		case 'd':
			countmax = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "-d %s: %s", optarg, errstr);
			break;
		case 'i':
			interactive = 1;
			break;
		case 'N':
			nflag = 0;
			break;
		case 'n':
			/* this is a noop, -n is the default */
			nflag = 1;
			break;
		case 's':
			delay = atof(optarg);
			if (delay <= 0)
				delay = 5;
			break;
		case 'w':
			rawwidth = strtonum(optarg, 1, MAX_LINE_BUF-1, &errstr);
			if (errstr)
				errx(1, "-w %s: %s", optarg, errstr);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (kd == NULL)
		warnx("kvm_openfiles: %s", errbuf);

	argc -= optind;
	argv += optind;

	if (argc == 1) {
		double del = atof(argv[0]);
		if (del == 0)
			viewstr = argv[0];
		else
			delay = del;
	} else if (argc == 2) {
		viewstr = argv[0];
		delay = atof(argv[1]);
		if (delay <= 0)
			delay = 5;
	}

	udelay = (useconds_t)(delay * 1000000.0);
	if (udelay < 1)
		udelay = 1;

	naptime = (double)udelay / 1000000.0;

	gethostname(hostname, sizeof (hostname));
	gethz();

	initialize();

	set_order(NULL);
	if (viewstr && set_view(viewstr)) {
		fprintf(stderr, "Unknown/ambiguous view name: %s\n", viewstr);
		return 1;
	}

	if (check_termcap()) {
		rawmode = 1;
		interactive = 0;
	}

	setup_term(maxlines);

	if (rawmode && countmax == 0)
		countmax = 1;

	gotsig_alarm = 1;

	engine_loop(countmax);

	return 0;
}

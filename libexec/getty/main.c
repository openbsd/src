/*	$OpenBSD: main.c,v 1.36 2015/04/14 02:24:17 millert Exp $	*/

/*-
 * Copyright (c) 1980, 1993
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
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <util.h>

#include "gettytab.h"
#include "pathnames.h"
#include "extern.h"

/*
 * Set the amount of running time that getty should accumulate
 * before deciding that something is wrong and exit.
 */
#define GETTY_TIMEOUT	60 /* seconds */

/* defines for auto detection of incoming PPP calls (->PAP/CHAP) */

#define PPP_FRAME	    0x7e  /* PPP Framing character */
#define PPP_STATION	    0xff  /* "All Station" character */
#define PPP_ESCAPE	    0x7d  /* Escape Character */
#define PPP_CONTROL	    0x03  /* PPP Control Field */
#define PPP_CONTROL_ESCAPED 0x23  /* PPP Control Field, escaped */
#define PPP_LCP_HI	    0xc0  /* LCP protocol - high byte */
#define PPP_LCP_LOW	    0x21  /* LCP protocol - low byte */

struct termios tmode, omode;

int crmod, digit, lower, upper;

char	hostname[HOST_NAME_MAX+1];
struct	utsname kerninfo;
char	name[LOGIN_NAME_MAX];
char	dev[] = _PATH_DEV;
char	ttyn[32];
char	*portselector(void);

#define	OBUFSIZ		128
#define	TABBUFSIZ	512

char	defent[TABBUFSIZ];
char	tabent[TABBUFSIZ];

char	*env[128];

char partab[] = {
	0001,0201,0201,0001,0201,0001,0001,0201,
	0202,0004,0003,0205,0005,0206,0201,0001,
	0201,0001,0001,0201,0001,0201,0201,0001,
	0001,0201,0201,0001,0201,0001,0001,0201,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0200,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0200,0000,0000,0200,0000,0200,0200,0000,
	0000,0200,0200,0000,0200,0000,0000,0201
};

#define	ERASE	tmode.c_cc[VERASE]
#define	KILL	tmode.c_cc[VKILL]
#define	EOT	tmode.c_cc[VEOF]

static void
dingdong(int signo)
{
	tmode.c_ispeed = tmode.c_ospeed = 0;
	(void)tcsetattr(0, TCSANOW, &tmode);
	_exit(1);
}

volatile sig_atomic_t interrupt_flag;

static void
interrupt(int signo)
{
	int save_errno = errno;

	interrupt_flag = 1;
	signal(SIGINT, interrupt);
	errno = save_errno;
}

/*
 * Action to take when getty is running too long.
 */
static void
timeoverrun(int signo)
{
	struct syslog_data sdata = SYSLOG_DATA_INIT;

	syslog_r(LOG_ERR, &sdata,
	    "getty exiting due to excessive running time");
	_exit(1);
}

static int	getname(void);
static void	oflush(void);
static void	prompt(void);
static void	putchr(int);
static void	putf(char *);
static void	putpad(char *);
static void	xputs(char *);

int
main(int argc, char *argv[])
{
	extern char **environ;
	char *tname;
	int repcnt = 0, failopenlogged = 0;
	struct rlimit limit;
	int rval;

	signal(SIGINT, SIG_IGN);
/*
	signal(SIGQUIT, SIG_DFL);
*/
	openlog("getty", LOG_ODELAY|LOG_CONS|LOG_PID, LOG_AUTH);
	gethostname(hostname, sizeof(hostname));
	if (hostname[0] == '\0')
		strlcpy(hostname, "Amnesiac", sizeof hostname);
	uname(&kerninfo);

	/*
	 * Limit running time to deal with broken or dead lines.
	 */
	(void)signal(SIGXCPU, timeoverrun);
	limit.rlim_max = RLIM_INFINITY;
	limit.rlim_cur = GETTY_TIMEOUT;
	(void)setrlimit(RLIMIT_CPU, &limit);

	/*
	 * The following is a work around for vhangup interactions
	 * which cause great problems getting window systems started.
	 * If the tty line is "-", we do the old style getty presuming
	 * that the file descriptors are already set up for us.
	 * J. Gettys - MIT Project Athena.
	 */
	if (argc <= 2 || strcmp(argv[2], "-") == 0) {
		if ((tname = ttyname(0)) == NULL) {
			syslog(LOG_ERR, "stdin: %m");
			exit(1);
		}
		if (strlcpy(ttyn, tname, sizeof(ttyn)) >= sizeof(ttyn)) {
			errno = ENAMETOOLONG;
			syslog(LOG_ERR, "%s: %m", tname);
			exit(1);
		}
	} else {
		int i;

		snprintf(ttyn, sizeof ttyn, "%s%s", dev, argv[2]);
		if (strcmp(argv[0], "+") != 0) {
			chown(ttyn, 0, 0);
			chmod(ttyn, 0600);
			revoke(ttyn);
			/*
			 * Delay the open so DTR stays down long enough to be detected.
			 */
			sleep(2);
			while ((i = open(ttyn, O_RDWR)) == -1) {
				if ((repcnt % 10 == 0) &&
				    (errno != ENXIO || !failopenlogged)) {
					syslog(LOG_ERR, "%s: %m", ttyn);
					closelog();
					failopenlogged = 1;
				}
				repcnt++;
				sleep(60);
			}
			login_tty(i);
		}
	}

	/* Start with default tty settings */
	if (tcgetattr(0, &tmode) < 0) {
		syslog(LOG_ERR, "%s: %m", ttyn);
		exit(1);
	}
	omode = tmode;

	gettable("default", defent);
	gendefaults();
	tname = "default";
	if (argc > 1)
		tname = argv[1];
	for (;;) {
		int off;

		gettable(tname, tabent);
		if (OPset || EPset || APset)
			APset++, OPset++, EPset++;
		setdefaults();
		off = 0;
		(void)tcflush(0, TCIOFLUSH);	/* clear out the crap */
		ioctl(0, FIONBIO, &off);	/* turn off non-blocking mode */
		ioctl(0, FIOASYNC, &off);	/* ditto for async mode */

		if (IS)
			cfsetispeed(&tmode, IS);
		else if (SP)
			cfsetispeed(&tmode, SP);
		if (OS)
			cfsetospeed(&tmode, OS);
		else if (SP)
			cfsetospeed(&tmode, SP);
		setflags(0);
		setchars();
		if (tcsetattr(0, TCSANOW, &tmode) < 0) {
			syslog(LOG_ERR, "%s: %m", ttyn);
			exit(1);
		}
		if (AB) {
			tname = autobaud();
			continue;
		}
		if (PS) {
			tname = portselector();
			continue;
		}
		if (CL && *CL)
			putpad(CL);
		edithost(HE);
		if (IM && *IM)
			putf(IM);
		if (TO) {
			signal(SIGALRM, dingdong);
			alarm(TO);
		}
		if ((rval = getname()) == 2) {
			oflush();
			alarm(0);
			signal(SIGALRM, SIG_DFL);
			execle(PP, "ppplogin", ttyn, (char *) 0, env);
			syslog(LOG_ERR, "%s: %m", PP);
			exit(1);
		} else if (rval) {
			int i;

			oflush();
			alarm(0);
			signal(SIGALRM, SIG_DFL);
			if (name[0] == '-') {
				xputs("user names may not start with '-'.");
				continue;
			}
			if (!(upper || lower || digit))
				continue;
			setflags(2);
			if (crmod) {
				tmode.c_iflag |= ICRNL;
				tmode.c_oflag |= ONLCR;
			}
			if (UC) {
				tmode.c_iflag |= IUCLC;
				tmode.c_oflag |= OLCUC;
				tmode.c_lflag |= XCASE;
			}
			if (lower || LC) {
				tmode.c_iflag &= ~IUCLC;
				tmode.c_oflag &= ~OLCUC;
				tmode.c_lflag &= ~XCASE;
			}
			if (tcsetattr(0, TCSANOW, &tmode) < 0) {
				syslog(LOG_ERR, "%s: %m", ttyn);
				exit(1);
			}
			signal(SIGINT, SIG_DFL);
			for (i = 0; environ[i] != (char *)0; i++)
				env[i] = environ[i];
			makeenv(&env[i]);

			limit.rlim_max = RLIM_INFINITY;
			limit.rlim_cur = RLIM_INFINITY;
			(void)setrlimit(RLIMIT_CPU, &limit);
			execle(LO, "login", "-p", "--", name, (char *)0, env);
			syslog(LOG_ERR, "%s: %m", LO);
			exit(1);
		}
		alarm(0);
		signal(SIGALRM, SIG_DFL);
		signal(SIGINT, SIG_IGN);
		if (NX && *NX)
			tname = NX;
	}
}

static int
getname(void)
{
	int ppp_state = 0, ppp_connection = 0;
	unsigned char cs;
	int c, r;
	char *np;

	/*
	 * Interrupt may happen if we use CBREAK mode
	 */
	signal(SIGINT, interrupt);
	setflags(1);
	prompt();
	if (PF > 0) {
		oflush();
		sleep(PF);
		PF = 0;
	}
	if (tcsetattr(0, TCSANOW, &tmode) < 0) {
		syslog(LOG_ERR, "%s: %m", ttyn);
		exit(1);
	}
	crmod = digit = lower = upper = 0;
	np = name;
	for (;;) {
		oflush();
		r = read(STDIN_FILENO, &cs, 1);
		if (r <= 0) {
			if (r == -1 && errno == EINTR && interrupt_flag) {
				interrupt_flag = 0;
				return (0);
			}
			exit(0);
		}
		if ((c = cs&0177) == 0)
			return (0);

		/*
		 * PPP detection state machine..
		 * Look for sequences:
		 * PPP_FRAME, PPP_STATION, PPP_ESCAPE, PPP_CONTROL_ESCAPED or
		 * PPP_FRAME, PPP_STATION, PPP_CONTROL (deviant from RFC)
		 * See RFC1662.
		 * Derived from code from Michael Hancock <michaelh@cet.co.jp>
		 * and Erik 'PPP' Olson <eriko@wrq.com>
		 */
		if (PP && cs == PPP_FRAME) {
			ppp_state = 1;
		} else if (ppp_state == 1 && cs == PPP_STATION) {
			ppp_state = 2;
		} else if (ppp_state == 2 && cs == PPP_ESCAPE) {
			ppp_state = 3;
		} else if ((ppp_state == 2 && cs == PPP_CONTROL) ||
		    (ppp_state == 3 && cs == PPP_CONTROL_ESCAPED)) {
			ppp_state = 4;
		} else if (ppp_state == 4 && cs == PPP_LCP_HI) {
			ppp_state = 5;
		} else if (ppp_state == 5 && cs == PPP_LCP_LOW) {
			ppp_connection = 1;
			break;
		} else {
			ppp_state = 0;
		}

		if (c == EOT)
			exit(1);
		if (c == '\r' || c == '\n' || np >= name + sizeof name -1) {
			putf("\r\n");
			break;
		}
		if (islower(c))
			lower = 1;
		else if (isupper(c))
			upper = 1;
		else if (c == ERASE || c == '#' || c == '\b') {
			if (np > name) {
				np--;
				if (cfgetospeed(&tmode) >= 1200)
					xputs("\b \b");
				else
					putchr(cs);
			}
			continue;
		} else if (c == KILL || c == '@') {
			putchr(cs);
			putchr('\r');
			if (cfgetospeed(&tmode) < 1200)
				putchr('\n');
			/* this is the way they do it down under ... */
			else if (np > name)
				xputs("                                     \r");
			prompt();
			np = name;
			continue;
		} else if (isdigit(c))
			digit++;
		if (IG && (c <= ' ' || c > 0176))
			continue;
		*np++ = c;
		putchr(cs);
	}
	signal(SIGINT, SIG_IGN);
	if (interrupt_flag) {
		interrupt_flag = 0;
		return (0);
	}
	*np = 0;
	if (c == '\r')
		crmod = 1;
	return (1 + ppp_connection);
}

static void
putpad(char *s)
{
	int pad = 0;
	speed_t ospeed = cfgetospeed(&tmode);

	if (isdigit((unsigned char)*s)) {
		while (isdigit((unsigned char)*s)) {
			pad *= 10;
			pad += *s++ - '0';
		}
		pad *= 10;
		if (*s == '.' && isdigit((unsigned char)s[1])) {
			pad += s[1] - '0';
			s += 2;
		}
	}

	xputs(s);
	/*
	 * If no delay needed, or output speed is
	 * not comprehensible, then don't try to delay.
	 */
	if (pad == 0 || ospeed <= 0)
		return;

	/*
	 * Round up by a half a character frame, and then do the delay.
	 * Too bad there are no user program accessible programmed delays.
	 * Transmitting pad characters slows many terminals down and also
	 * loads the system.
	 */
	pad = (pad * ospeed + 50000) / 100000;
	while (pad--)
		putchr(*PC);
}

static void
xputs(char *s)
{
	while (*s)
		putchr(*s++);
}

char	outbuf[OBUFSIZ];
int	obufcnt = 0;

static void
putchr(int cc)
{
	char c;

	c = cc;
	if (!NP) {
		c |= partab[c&0177] & 0200;
		if (OP)
			c ^= 0200;
	}
	if (!UB) {
		outbuf[obufcnt++] = c;
		if (obufcnt >= OBUFSIZ)
			oflush();
	} else
		write(STDOUT_FILENO, &c, 1);
}

static void
oflush(void)
{
	if (obufcnt)
		write(STDOUT_FILENO, outbuf, obufcnt);
	obufcnt = 0;
}

static void
prompt(void)
{

	putf(LM);
	if (CO)
		putchr('\n');
}

static void
putf(char *cp)
{
	extern char editedhost[];
	char *slash, db[100];
	time_t t;

	while (*cp) {
		if (*cp != '%') {
			putchr(*cp++);
			continue;
		}
		switch (*++cp) {

		case 't':
			slash = strrchr(ttyn, '/');
			if (slash == (char *) 0)
				xputs(ttyn);
			else
				xputs(&slash[1]);
			break;

		case 'h':
			xputs(editedhost);
			break;

		case 'd': {
			(void)time(&t);
			(void)strftime(db, sizeof(db),
			    "%l:%M%p on %A, %d %B %Y", localtime(&t));
			xputs(db);
			break;
		}

		case 's':
			xputs(kerninfo.sysname);
			break;

		case 'm':
			xputs(kerninfo.machine);
			break;

		case 'r':
			xputs(kerninfo.release);
			break;

		case 'v':
			xputs(kerninfo.version);
			break;

		case '%':
			putchr('%');
			break;
		}
		cp++;
	}
}

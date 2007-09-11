/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)sliplogin.c	5.6 (Berkeley) 3/2/91";*/
static char rcsid[] = "$Id: sliplogin.c,v 1.27 2007/09/11 16:30:59 gilles Exp $";
#endif /* not lint */

/*
 * sliplogin.c
 * [MUST BE RUN SUID, SLOPEN DOES A SUSER()!]
 *
 * This program initializes its own tty port to be an async TCP/IP interface.
 * It sets the line discipline to slip, invokes a shell script to initialize
 * the network interface, then pauses forever waiting for hangup.
 *
 * It is a remote descendant of several similar programs with incestuous ties:
 * - Kirk Smith's slipconf, modified by Richard Johnsson @ DEC WRL.
 * - slattach, probably by Rick Adams but touched by countless hordes.
 * - the original sliplogin for 4.2bsd, Doug Kingston the mover behind it.
 *
 * There are two forms of usage:
 *
 * "sliplogin"
 * Invoked simply as "sliplogin", the program looks up the username
 * in the file /etc/slip.hosts.
 * If an entry is found, the line on fd0 is configured for SLIP operation
 * as specified in the file.
 *
 * "sliplogin IPhostlogin </dev/ttyb"
 * Invoked by root with a username, the name is looked up in the
 * /etc/slip.hosts file and if found fd0 is configured as in case 1.
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>

#include <termios.h>
#include <sys/ioctl.h>
#include <ttyent.h>
#include <net/slip.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

extern char **environ;

static char *restricted_environ[] = {
	"PATH=" _PATH_STDPATH,
	NULL
};

int	unit;
int	speed;
uid_t	uid;
char	loginargs[BUFSIZ];
char	loginfile[MAXPATHLEN];
char	loginname[BUFSIZ];

void
findid(char *name)
{
	FILE *fp;
	static char slopt[5][16];
	static char laddr[16];
	static char raddr[16];
	static char mask[16];
	char user[MAXLOGNAME], *p;
	int n;

	strlcpy(loginname, name, sizeof loginname);
	if ((fp = fopen(_PATH_ACCESS, "r")) == NULL) {
		syslog(LOG_ERR, "%s: %m", _PATH_ACCESS);
		err(1, "%s", _PATH_ACCESS);
	}
	while (fgets(loginargs, sizeof(loginargs), fp)) {
		if (ferror(fp))
			break;
		loginargs[strcspn(loginargs, "#")] = '\0';
		loginargs[strcspn(loginargs, "\n")] = '\0';
		n = sscanf(loginargs, "%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s%*[ \t]%15s\n",
		    user, laddr, raddr, mask, slopt[0], slopt[1],
		    slopt[2], slopt[3], slopt[4]);
		if (strcmp(user, name) != 0)
			continue;

		/*
		 * we've found the guy we're looking for -- see if
		 * there's a login file we can use.  First check for
		 * one specific to this host.  If none found, try for
		 * a generic one.
		 */
		(void)snprintf(loginfile, sizeof loginfile, "%s.%s",
		    _PATH_LOGIN, name);
		if (access(loginfile, R_OK|X_OK) != 0) {
			(void)strlcpy(loginfile, _PATH_LOGIN, sizeof(loginfile));
			if (access(loginfile, R_OK|X_OK)) {
				fputs("access denied - no login file\n",
				    stderr);
				syslog(LOG_ERR,
				    "access denied for %s - no %s",
				    name, _PATH_LOGIN);
				exit(5);
			}
		}

		(void) fclose(fp);
		return;
	}
	syslog(LOG_ERR, "SLIP access denied for %s", name);
	errx(1, "SLIP access denied for %s", name);
	/* NOTREACHED */
}

const char *
sigstr(int s)
{
	if (s > 0 && s < NSIG)
		return(sys_signame[s]);
	else {
		static char buf[32];
		(void)snprintf(buf, sizeof buf, "sig %d", s);
		return(buf);
	}
}

volatile sig_atomic_t die;

/* ARGSUSED */
void
hup_handler(int signo)
{
	die = 1;
}

int
main(int argc, char *argv[])
{
	int fd, s, ldisc, odisc;
	char *name;
	struct termios tios, otios;
	char logoutfile[MAXPATHLEN], logincmd[2*BUFSIZ+32];
	sigset_t emptyset;

	environ = restricted_environ; /* minimal protection for system() */

	if ((name = strrchr(argv[0], '/')) == NULL)
		name = argv[0];
	else
		name++;
	closefrom(3);
	if (argc > 1 && strlen(argv[1]) > MAXLOGNAME)
		errx(1, "login %s too long", argv[1]);
	openlog(name, LOG_PID, LOG_DAEMON);
	uid = getuid();
	if (argc > 1) {
		findid(argv[1]);

		/*
		 * Disassociate from current controlling terminal, if any,
		 * and ensure that the slip line is our controlling terminal.
		 */
		switch (fork()) {
		case -1:
			perror("fork");
			exit(1);
		case 0:
			break;
		default:
			exit(0);
		}
		if (setsid() == -1)
			perror("setsid");
		if (argc > 2) {
			if ((fd = open(argv[2], O_RDWR)) == -1) {
				perror(argv[2]);
				exit(2);
			}
			(void) dup2(fd, 0);
			if (fd > 2)
				close(fd);
		}
#ifdef TIOCSCTTY
		if (ioctl(STDIN_FILENO, TIOCSCTTY, (caddr_t)0) != 0)
			perror("ioctl (TIOCSCTTY)");
#endif
	} else {
		if ((name = getlogin()) == NULL) {
			syslog(LOG_ERR,
			    "access denied - getlogin returned 0");
			errx(1, "access denied - no username");
		}
		findid(name);
	}
	if (!isatty(STDIN_FILENO)) {
		syslog(LOG_ERR, "stdin not a tty");
		errx(1, "stdin not a tty");
	}
	(void) fchmod(STDIN_FILENO, 0600);
	warnx("starting slip login for %s", loginname);
	/* set up the line parameters */
	if (tcgetattr(STDIN_FILENO, &tios) < 0) {
		syslog(LOG_ERR, "tcgetattr: %m");
		exit(1);
	}
	otios = tios;
	cfmakeraw(&tios);
	tios.c_iflag &= ~IMAXBEL;
	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &tios) < 0) {
		syslog(LOG_ERR, "tcsetattr: %m");
		exit(1);
	}
	speed = cfgetispeed(&tios);
	/* find out what ldisc we started with */
	if (ioctl(STDIN_FILENO, TIOCGETD, (caddr_t)&odisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCGETD) (1): %m");
		exit(1);
	}
	ldisc = SLIPDISC;
	if (ioctl(STDIN_FILENO, TIOCSETD, (caddr_t)&ldisc) < 0) {
		syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
		exit(1);
	}
	/* find out what unit number we were assigned */
	if (ioctl(STDIN_FILENO, SLIOCGUNIT, (caddr_t)&unit) < 0) {
		syslog(LOG_ERR, "ioctl (SLIOCGUNIT): %m");
		exit(1);
	}
	(void) signal(SIGHUP, hup_handler);
	(void) signal(SIGTERM, hup_handler);

	syslog(LOG_INFO, "attaching slip unit %d for %s", unit, loginname);
	(void)snprintf(logincmd, sizeof(loginargs), "%s %d %d %s", loginfile,
	    unit, speed, loginargs);

	/*
	 * aim stdout and errout at /dev/null so logincmd output won't
	 * babble into the slip tty line.
	 */
	(void) close(1);
	if ((fd = open(_PATH_DEVNULL, O_WRONLY)) != 1) {
		if (fd < 0) {
			syslog(LOG_ERR, "open /dev/null: %m");
			exit(1);
		}
		(void) dup2(fd, 1);
		(void) close(fd);
	}
	(void) dup2(1, 2);

	/*
	 * Run login and logout scripts as root (real and effective);
	 * current route(8) is setuid root, and checks the real uid
	 * to see whether changes are allowed (or just "route get").
	 */
	(void) setuid(0);
	if ((s = system(logincmd))) {
		syslog(LOG_ERR, "%s login failed: exit status %d from %s",
		    loginname, s, loginfile);
		(void) ioctl(STDIN_FILENO, TIOCSETD, (caddr_t)&odisc);
		(void) tcsetattr(STDIN_FILENO, TCSAFLUSH, &otios);
		exit(6);
	}

	/* twiddle thumbs until we get a signal; allow user to kill */
	seteuid(uid);
	sigemptyset(&emptyset);
	while (die == 0)
		sigsuspend(&emptyset);

	seteuid(0);
	(void)snprintf(logoutfile, sizeof logoutfile, "%s.%s",
	    _PATH_LOGOUT, loginname);
	if (access(logoutfile, R_OK|X_OK) != 0)
		(void)strlcpy(logoutfile, _PATH_LOGOUT,
		    sizeof(logoutfile));
	if (access(logoutfile, R_OK|X_OK) == 0) {
		char logincmd[2*MAXPATHLEN+32];

		(void) snprintf(logincmd, sizeof logincmd, "%s %d %d %s",
		    logoutfile, unit, speed, loginargs);
		(void) system(logincmd);
	}

	close(0);
	syslog(LOG_INFO, "closed %s slip unit %d (%s)",
	    loginname, unit, sigstr(s));
	exit(1);
}

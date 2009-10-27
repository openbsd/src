/*	$OpenBSD: slattach.c,v 1.16 2009/10/27 23:59:34 deraadt Exp $	*/
/*	$NetBSD: slattach.c,v 1.17 1996/05/19 21:57:39 jonathan Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Adams.
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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

int	speed = 9600;
int	slipdisc = SLIPDISC;

char	devicename[32];

static char pidfilename[MAXPATHLEN];	/* name of pid file */
static pid_t pid;			/* Our pid */
static FILE *pidfile;

void	usage(void);
int ttydisc(char *);
void handler(int);

volatile sig_atomic_t dying;

int
main(int argc, char *argv[])
{
	int fd;
	char *dev;
	struct termios tty;
	tcflag_t cflag = HUPCL;
	int ch;
	sigset_t sigset;
	int i;

	while ((ch = getopt(argc, argv, "hms:r:t:")) != -1) {
		switch (ch) {
		case 'h':
			cflag |= CRTSCTS;
			break;
		case 'm':
			cflag &= ~HUPCL;
			break;
		case 's':
			speed = atoi(optarg);
			break;
		case 'r': case 't':
			slipdisc = ttydisc(optarg);
			break;
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	dev = *argv;
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		(void)snprintf(devicename, sizeof(devicename),
		    "%s%s", _PATH_DEV, dev);
		dev = devicename;
	}
	if ((fd = open(dev, O_RDWR | O_NDELAY)) < 0)
		err(1, "open: %s", dev);

	tty.c_cflag = CREAD | CS8 | cflag;
	tty.c_iflag = 0;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	cfsetspeed(&tty, speed);
	if (tcsetattr(fd, TCSADRAIN, &tty) < 0)
		err(1, "tcsetattr");
	if (ioctl(fd, TIOCSDTR, 0) < 0)
		err(1, "TIOCSDTR");
	if (ioctl(fd, TIOCSETD, &slipdisc) < 0)
		err(1, "TIOCSETD");

	if (fork() > 0)
		return (0);

	/* set up a signal handler to delete the PID file. */
	signal(SIGHUP, handler);
	signal(SIGTERM, handler);

	/* write PID to a file */
	pid = getpid();

	for(i = strlen(dev); (dev[i] != '/') && i > 0; i--)
		;
	if(dev[i] == '/')
		i++;
	(void) snprintf(pidfilename, sizeof pidfilename,
	    "%sslip.%s.pid", _PATH_VARRUN, dev + i);
	truncate(pidfilename, 0); /* If this fails, so will the next one... */
	if ((pidfile = fopen(pidfilename, "w")) != NULL) {
		fprintf(pidfile, "%ld\n", (long)pid);
		(void) fclose(pidfile);
	} else {
		syslog(LOG_ERR, "Failed to create pid file %s: %m", pidfilename);
		pidfilename[0] = 0;
	}

	sigemptyset(&sigset);
	for (;;) {
		sigsuspend(&sigset);
		if (dying) {
			/*  delete the pid file.  */
			if (pidfilename[0] != 0) {
				if (unlink(pidfilename) < 0 && errno != ENOENT)
					syslog(LOG_WARNING,
					    "unable to delete pid file: %m");
			}

			/* terminate gracefully */
			return (0);
		}
	}
}

void
handler(int useless)
{
	dying = 1;
}

int
ttydisc(char *name)
{
	if (strcmp(name, "slip") == 0)
		return(SLIPDISC);
#ifdef STRIPDISC
	else if (strcmp(name, "strip") == 0)
		return(STRIPDISC);
#endif
	else
		usage();
}

void
usage(void)
{

	fprintf(stderr,
	    "usage: slattach [-t ldisc] [-hm] [-s baudrate] ttyname\n");
	exit(1);
}

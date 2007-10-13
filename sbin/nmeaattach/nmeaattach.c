/*	$OpenBSD: nmeaattach.c,v 1.9 2007/10/13 16:28:24 mbalmer Exp $	*/
/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2006 Marc Balmer <mbalmer@openbsd.org>
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

#include <sys/ioctl.h>
#include <sys/ttycom.h>

#include <err.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

int	speed = B4800;

char	devicename[32];

__dead void	usage(void);
void 		coroner(int);

volatile sig_atomic_t dying = 0;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-dhm] [-s baudrate] [-t cond] device\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dev;
	struct termios tty;
	struct tstamps tstamps;
	const char *errstr;
	tcflag_t cflag = HUPCL;
	int ch;
	sigset_t sigset;
	int nmeadisc = NMEADISC;
	int nodaemon = 0;

	tstamps.ts_set = tstamps.ts_clr = 0;

	while ((ch = getopt(argc, argv, "dhms:t:")) != -1) {
		switch (ch) {
		case 'd':
			nodaemon = 1;
			break;
		case 'h':
			cflag |= CRTSCTS;
			break;
		case 'm':
			cflag &= ~HUPCL;
			break;
		case 's':
			speed = (int)strtonum(optarg, 50, 115200, &errstr);
			if (errstr)
				errx(1, "speed is %s: %s", errstr, optarg);
			break;
		case 't':
			if (!strcasecmp(optarg, "dcd"))
				tstamps.ts_set |= TIOCM_CAR;
			else if (!strcasecmp(optarg, "!dcd"))
				tstamps.ts_clr |= TIOCM_CAR;
			else if (!strcasecmp(optarg, "cts"))
				tstamps.ts_set |= TIOCM_CTS;
			else if (!strcasecmp(optarg, "!cts"))
				tstamps.ts_clr |= TIOCM_CTS;
			else
				errx(1, "'%s' not supported for timestamping",
				    optarg);
			break;
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
	if ((fd = open(dev, O_RDWR)) < 0)
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
		warn("TIOCSDTR");
	if (ioctl(fd, TIOCSETD, &nmeadisc) < 0)
		err(1, "TIOCSETD");
	if (ioctl(fd, TIOCSTSTAMP, &tstamps) < 0)
		err(1, "TIOCSTSTAMP");

	if (!nodaemon && daemon(0, 0))
		errx(1, "can't daemonize");

	signal(SIGHUP, coroner);
	signal(SIGTERM, coroner);

	sigemptyset(&sigset);
	for (;;) {
		sigsuspend(&sigset);
		if (dying)
			return 0;
	}
}

/* ARGSUSED */
void
coroner(int useless)
{
	dying = 1;
}

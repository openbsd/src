/*	$OpenBSD: ldattach.c,v 1.1.1.1 2007/11/03 15:22:54 mbalmer Exp $	*/

/*
 * Copyright (c) 2007 Marc Balmer <mbalmer@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * A replacement for slattach(8) and nmeaattach(8) that can be used from
 * the commandline or from init(8) (using entries in /etc/ttys).
 */

#include <sys/ioctl.h>
#include <sys/ttycom.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

__dead void	usage(void);
void 		coroner(int);

volatile sig_atomic_t dying = 0;

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-7dehmo] [-s baudrate] "
	    "[-t cond] discipline device\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct termios tty;
	struct tstamps tstamps;
	const char *errstr;
	tcflag_t cflag = HUPCL | CS8;
	sigset_t sigset;
	pid_t ppid;
	int ch, fd, ldisc, nodaemon = 0, speed = B4800, parity = 0;
	char devname[32], *dev, *disc;

	tstamps.ts_set = tstamps.ts_clr = 0;

	if ((ppid = getppid()) == 1)
		nodaemon = 1;

	while ((ch = getopt(argc, argv, "7dehmos:t:")) != -1) {
		switch (ch) {
		case '7':
			cflag &= ~CS8;
			cflag |= CS7;
			break;
		case 'd':
			nodaemon = 1;
			break;
		case 'e':
			if (parity != 0)
				parity = 0;	/* -o -e */
			else
				parity = -1;	/* even */
			break;
		case 'h':
			cflag |= CRTSCTS;
			break;
		case 'm':
			cflag &= ~HUPCL;
			break;
		case 'o':
			if (parity != 0)
				parity = 0;	/* -e -o */
			else
				parity = 1;	/* odd */
			break;
		case 's':
			speed = (int)strtonum(optarg, 50, 230400, &errstr);
			if (errstr) {
				if (ppid != 1)
					errx(1,  "speed is %s: %s", errstr,
					    optarg);
				else
					goto bail_out;
			}
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
			else {
				if (ppid != 1)
					errx(1, "'%s' not supported for "
					    "timestamping", optarg);
				else
					goto bail_out;
			}
			break;
		default:
			if (ppid != -1)
				usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (ppid != 1 && argc != 2)
		usage();

	disc = *argv++;
	dev = *argv;
	if (strncmp(_PATH_DEV, dev, sizeof(_PATH_DEV) - 1)) {
		(void)snprintf(devname, sizeof(devname),
		    "%s%s", _PATH_DEV, dev);
		dev = devname;
	}
	syslog(LOG_INFO, "attach %s on %s", disc, dev);
	if ((fd = open(dev, O_RDWR)) < 0)
		goto bail_out;

	if (!strcmp(disc, "slip"))
		ldisc = SLIPDISC;
	else if (!strcmp(disc, "nmea"))
		ldisc = NMEADISC;
	else {
		syslog(LOG_ERR, "unknown line discipline %s", disc);
		goto bail_out;
	}

	switch (parity) {
	case -1:	/* even parity */
		cflag |= PARENB;
		break;
	case 1:		/* odd parity */
		cflag |= PARENB | PARODD;
		break;
	}
	
	tty.c_cflag = CREAD | cflag;
	tty.c_iflag = 0;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 0;
	cfsetspeed(&tty, speed);
	if (tcsetattr(fd, TCSADRAIN, &tty) < 0) {
		if (ppid != 1)
			warnx("tcsetattr");
		goto bail_out;
	}

	/* setup common to all line disciplines */
	if (ioctl(fd, TIOCSDTR, 0) < 0)
		warn("TIOCSDTR");
	if (ioctl(fd, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR, "can't set the %s line discipline on %s", disc,
		    dev);
		goto bail_out;
	}

	/* line discpline specific setup */
	switch (ldisc) {
	case NMEADISC:
		if (ioctl(fd, TIOCSTSTAMP, &tstamps) < 0) {
			warnx("TIOCSTSTAMP");
			goto bail_out;
		}
		break;
	}

	if (!nodaemon && daemon(0, 0))
		errx(1, "can't daemonize");

	signal(SIGHUP, coroner);
	signal(SIGTERM, coroner);

	sigemptyset(&sigset);
	while (!dying)
		sigsuspend(&sigset);

bail_out:
	if (ppid == 1)
		sleep(30);	/* delay restart when called from init */

	return 0;
}

/* ARGSUSED */
void
coroner(int useless)
{
	dying = 1;
}

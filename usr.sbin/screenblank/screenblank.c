/*	$OpenBSD: screenblank.c,v 1.5 1998/06/03 17:00:09 deraadt Exp $	*/
/*	$NetBSD: screenblank.c,v 1.2 1996/02/28 01:18:34 thorpej Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Screensaver daemon for the Sun 3 and SPARC.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <paths.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include <machine/fbio.h>

#include "pathnames.h"

struct	dev_stat {
	LIST_ENTRY(dev_stat) ds_link;	/* linked list */
	char	*ds_path;		/* path to device */
	int	ds_isfb;		/* boolean; framebuffer? */
	time_t	ds_atime;		/* time device last accessed */
	time_t	ds_mtime;		/* time device last modified */
};
LIST_HEAD(ds_list, dev_stat) ds_list;

extern	char *__progname;

static	void add_dev __P((char *, int));
static	void change_state __P((int));
static	void cvt_arg __P((char *, struct timeval *));
static	void logpid __P((void));
static	void sighandler __P((int));
static	void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct dev_stat *dsp;
	struct timeval timo_on, timo_off, *tvp;
	struct sigaction sa;
	struct stat st;
	int ch, change, fflag = 0, kflag = 0, mflag = 0, state;

	LIST_INIT(&ds_list);

	/*
	 * Set the default timeouts: 10 minutes on, .25 seconds off.
	 */
	timo_on.tv_sec = 600;
	timo_on.tv_usec = 0;
	timo_off.tv_sec = 0;
	timo_off.tv_usec = 250000;

	while ((ch = getopt(argc, argv, "d:e:f:km")) != -1) {
		switch (ch) {
		case 'd':
			cvt_arg(optarg, &timo_on);
			break;

		case 'e':
			cvt_arg(optarg, &timo_off);
			break;

		case 'f':
			fflag = 1;
			add_dev(optarg, 1);
			break;

		case 'k':
			if (mflag || kflag)
				usage();
			kflag = 1;
			break;

		case 'm':
			if (kflag || mflag)
				usage();
			mflag = 1;
			break;

		default:
			usage();
		}
	}
	argc -= optind;
	if (argc)
		usage();

	/*
	 * Add the keyboard, mouse, and default framebuffer devices
	 * as necessary.  We _always_ check the console device.
	 */
	add_dev(_PATH_CONSOLE, 0);
	if (!kflag)
		add_dev(_PATH_KEYBOARD, 0);
	if (!mflag)
		add_dev(_PATH_MOUSE, 0);
	if (!fflag)
		add_dev(_PATH_FB, 1);

	/* Ensure that the framebuffer is on. */
	state = FBVIDEO_ON;
	change_state(state);
	tvp = &timo_on;

	/*
	 * Make sure the framebuffer gets turned back on when we're
	 * killed.
	 */
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = sighandler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_NOCLDSTOP;
	if (sigaction(SIGINT, &sa, NULL) || sigaction(SIGTERM, &sa, NULL) ||
	    sigaction(SIGHUP, &sa, NULL))
		err(1, "sigaction");

	/* Detach. */
	if (daemon(0, 0))
		err(1, "daemon");
	logpid();

	/* Start the state machine. */
	for (;;) {
		change = 0;
		for (dsp = ds_list.lh_first; dsp != NULL;
		    dsp = dsp->ds_link.le_next) {
			/* Don't check framebuffers. */
			if (dsp->ds_isfb)
				continue;
			if (stat(dsp->ds_path, &st) < 0)
				err(1, "stat: %s", dsp->ds_path);
			if (st.st_atime > dsp->ds_atime) {
				change = 1;
				dsp->ds_atime = st.st_atime;
			}
			if (st.st_mtime > dsp->ds_mtime) {
				change = 1;
				dsp->ds_mtime = st.st_mtime;
			}
		}

		switch (state) {
		case FBVIDEO_ON:
			if (!change) {
				state = FBVIDEO_OFF;
				change_state(state);
				tvp = &timo_off;
			}
			break;

		case FBVIDEO_OFF:
			if (change) {
				state = FBVIDEO_ON;
				change_state(state);
				tvp = &timo_on;
			}
			break;
		}

		if (select(0, NULL, NULL, NULL, tvp) < 0)
			err(1, "select");
	}
	/* NOTREACHED */
}

static void
add_dev(path, isfb)
	char *path;
	int isfb;
{
	struct dev_stat *dsp1, *dsp2;

	/* Create the entry... */
	dsp1 = malloc(sizeof(struct dev_stat));
	if (dsp1 == NULL)
		errx(1, "can't allocate memory for %s", path);
	bzero(dsp1, sizeof(struct dev_stat));
	dsp1->ds_path = path;
	dsp1->ds_isfb = isfb;

	/* ...and put it in the list. */
	if (ds_list.lh_first == NULL) {
		LIST_INSERT_HEAD(&ds_list, dsp1, ds_link);
	} else {
		for (dsp2 = ds_list.lh_first; dsp2->ds_link.le_next != NULL;
		    dsp2 = dsp2->ds_link.le_next)
			/* Nothing. */ ;
		LIST_INSERT_AFTER(dsp2, dsp1, ds_link);
	}
}

/* ARGSUSED */
static void
sighandler(sig)
	int sig;
{

	/* Kill the pid file and re-enable the framebuffer before exit. */
	(void)unlink(_PATH_SCREENBLANKPID);
	change_state(FBVIDEO_ON);
	exit(0);
}

static void
change_state(state)
	int state;
{
	struct dev_stat *dsp;
	int fd;

	for (dsp = ds_list.lh_first; dsp != NULL; dsp = dsp->ds_link.le_next) {
		/* Don't change the state of non-framebuffers! */
		if (dsp->ds_isfb == 0)
			continue;
		if ((fd = open(dsp->ds_path, O_RDWR, 0)) < 0) {
			warn("open: %s", dsp->ds_path);
			continue;
		}
		if (ioctl(fd, FBIOSVIDEO, &state) < 0)
			warn("ioctl: %s", dsp->ds_path);
		(void)close(fd);
	}
}

static void
cvt_arg(arg, tvp)
	char *arg;
	struct timeval *tvp;
{
	char *cp;
	double seconds = 0.0, exponent = -1.0;
	int period = 0;

	for (cp = arg; *cp != '\0'; ++cp) {
		if (*cp == '.') {
			if (period)
				errx(1, "invalid argument: %s", arg);
			period = 1;
			continue;
		}

		if (!isdigit(*cp))
			errx(1, "invalid argument: %s", arg);

		if (period) {
			seconds = seconds + ((*cp - '0') * pow(10.0, exponent));
			exponent -= 1.0;
		} else
			seconds = (seconds * 10.0) + (*cp - '0');
	}

	tvp->tv_sec = (long)seconds;
	tvp->tv_usec = (long)((seconds - tvp->tv_sec) * 1000000);
}

static void
logpid()
{
	FILE *fp;

	if ((fp = fopen(_PATH_SCREENBLANKPID, "w")) != NULL) {
		fprintf(fp, "%u\n", getpid());
		(void)fclose(fp);
	}
}

static void
usage()
{

	fprintf(stderr, "usage: %s [-k | -m] [-d timeout] [-e timeout] %s\n",
	    __progname, "[-f framebuffer]");
	exit(1);
}

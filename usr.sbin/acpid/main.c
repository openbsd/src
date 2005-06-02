/*	$OpenBSD: main.c,v 1.1 2005/06/02 20:09:39 tholo Exp $	*/
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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

#include <sys/types.h>
#include <sys/event.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <err.h>
#include <syslog.h>
#include <machine/bus.h>
#include <sys/device.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

#include "pathnames.h"
#include "acpi.h"

void sigexit(int);
void usage(void);
void run_script(const char *);

int debug = 0;

const char acpidev[] = _PATH_ACPI_DEV;

extern char *__progname;

void
sigexit(int sig)
{
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-d]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *fname = acpidev;
	int acpi_fd, ch;
	int kq;
	struct kevent ev[2];

	while ((ch = getopt(argc, argv, "qadsepmf:t:S:")) != -1)
		switch(ch) {
		case 'd':
			debug = 1;
			break;
		case '?':
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (debug)
		openlog(__progname, LOG_CONS, LOG_LOCAL1);
	else {
		daemon(0, 0);
		openlog(__progname, LOG_CONS, LOG_DAEMON);
		setlogmask(LOG_UPTO(LOG_NOTICE));
	}

	(void) signal(SIGTERM, sigexit);
	(void) signal(SIGHUP, sigexit);
	(void) signal(SIGINT, sigexit);

	if ((acpi_fd = open(fname, O_RDONLY)) == -1)
		err(1, "open");

	if (fcntl(acpi_fd, F_SETFD, 1) == -1)
		err(1, "fcntl");

	kq = kqueue();
	if (kq <= 0)
		err(1, "kqueue");

	EV_SET(&ev[0], acpi_fd, EVFILT_READ, EV_ADD | EV_ENABLE | EV_CLEAR,
	    0, 0, NULL);
	if (kevent(kq, ev, 1, NULL, 0, NULL) < 0)
		err(1, "kevent");

	for (;;) {
		int rv;

		if ((rv = kevent(kq, NULL, 0, ev, 1, NULL)) < 0)
			break;

		if (!rv)
			continue;

		if (ev->ident == (u_int)acpi_fd) {
			syslog(LOG_DEBUG, "acpi event %04x index %d",
			    ACPI_EVENT_TYPE(ev->data),
			    ACPI_EVENT_INDEX(ev->data));

			switch (ACPI_EVENT_TYPE(ev->data)) {
			case ACPI_EV_PWRBTN:
				run_script("power-button");
				break;
			case ACPI_EV_SLPBTN:
				run_script("sleep-button");
				break;
			default:
				break;
			}

		}
	}
	err(1, "kevent");
}

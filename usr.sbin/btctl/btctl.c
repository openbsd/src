/*	$OpenBSD: btctl.c,v 1.2 2008/11/26 22:17:18 uwe Exp $	*/

/*
 * Copyright (c) 2008 Uwe Stuehler
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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

#include <sys/un.h>

#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "btd.h"
#include "btctl.h"

__dead void usage(void);

static const char *progname;

__dead void
usage(void)
{
	fprintf(stderr, "usage: %s -f file\n", progname);
	exit(2);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un sun;
	const char *conffile;
	int ctl_sock;
	int ch;
	int try;

	progname = basename(argv[0]);
	conffile = NULL;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			conffile = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (conffile != NULL && argc > 0)
		usage();

	if (conffile == NULL)
		usage();

	log_init(1);

	if ((ctl_sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
		err(1, "socket");

	bzero(&sun, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strlcpy(sun.sun_path, BTD_SOCKET, sizeof(sun.sun_path));

	try = 0;
 reconnect:
	/* connect to btd control socket, retry a few times */
	if (connect(ctl_sock, (struct sockaddr *)&sun, sizeof(sun)) == -1) {
		/* Keep retrying if running in monitor mode */
		if ((errno == ENOENT || errno == ECONNREFUSED) &&
		    ++try < 50) {
			usleep(100);
			goto reconnect;
		}
		err(1, "%s", BTD_SOCKET);
	}

	if (parse_config(conffile, ctl_sock))
		exit(1);

	return 0;
}

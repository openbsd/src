/*	$OpenBSD: cu.c,v 1.37 2010/07/03 03:33:12 nicm Exp $	*/
/*	$NetBSD: cu.c,v 1.5 1997/02/11 09:24:05 mrg Exp $	*/

/*
 * Copyright (c) 1983, 1993
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

#include <err.h>
#include <paths.h>
#include <util.h>

#include "tip.h"

static void	cuusage(void);

/*
 * Botch the interface to look like cu's
 */
void
cumain(int argc, char *argv[])
{
	int ch, i, parity, baudrate;
	const char *errstr;
	static char sbuf[12];
	char *device;

	if (argc < 2)
		cuusage();
	vsetnum(BAUDRATE, DEFBR);
	parity = 0;	/* none */

	/*
	 * Convert obsolecent -### speed to modern -s### syntax which
	 * getopt() can handle.
	 */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				ch = snprintf(sbuf, sizeof(sbuf), "-s%s",
				    &argv[i][1]);
				if (ch <= 0 || ch >= sizeof(sbuf)) {
					errx(3, "invalid speed: %s",
					    &argv[i][1]);
				}
				argv[i] = sbuf;
				break;
			case '-':
				/* if we get "--" stop processing args */
				if (argv[i][2] == '\0')
					goto getopt;
				break;
			}
		}
	}

getopt:
	while ((ch = getopt(argc, argv, "l:s:htoe")) != -1) {
		switch (ch) {
		case 'l':
			if (vgetstr(DEVICE) != NULL) {
				fprintf(stderr,
				    "%s: cannot specify multiple -l options\n",
				    __progname);
				exit(3);
			}
			if (strchr(optarg, '/'))
				vsetstr(DEVICE, optarg);
			else {
				if (asprintf(&device,
				    "%s%s", _PATH_DEV, optarg) == -1)
					err(3, "asprintf");
				vsetstr(DEVICE, device);
			}
			break;
		case 's':
			baudrate = (int)strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr)
				errx(3, "speed is %s: %s", errstr, optarg);
			vsetnum(BAUDRATE, baudrate);
			break;
		case 'h':
			vsetnum(LECHO, 1);
			vsetnum(HALFDUPLEX, 1);
			break;
		case 't':
			/* Was for a hardwired dial-up connection. */
			break;
		case 'o':
			if (parity != 0)
				parity = 0;	/* -e -o */
			else
				parity = 1;	/* odd */
			break;
		case 'e':
			if (parity != 0)
				parity = 0;	/* -o -e */
			else
				parity = -1;	/* even */
			break;
		default:
			cuusage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	switch (argc) {
	case 1:
		/* Was phone number but now ignored. */
	case 0:
		break;
	default:
		cuusage();
		break;
	}

	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);
	signal(SIGCHLD, SIG_DFL);

	/*
	 * The "cu" host name is used to define the
	 * attributes of the generic dialer.
	 */
	snprintf(sbuf, sizeof(sbuf), "cu%d", vgetnum(BAUDRATE));
	FD = hunt(sbuf);
	setbuf(stdout, NULL);

	loginit();

	switch (parity) {
	case -1:
		setparity("even");
		break;
	case 1:
		setparity("odd");
		break;
	default:
		setparity("none");
		break;
	}
	vsetnum(VERBOSE, 0);
	if (ttysetup(vgetnum(BAUDRATE))) {
		fprintf(stderr, "%s: unsupported speed %d\n",
		    __progname, vgetnum(BAUDRATE));
		(void)uu_unlock(uucplock);
		exit(3);
	}
	con();
}

static void
cuusage(void)
{
	fprintf(stderr, "usage: cu [-eho] [-l line] [-s speed | -speed]\n");
	exit(8);
}

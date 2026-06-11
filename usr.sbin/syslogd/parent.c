/*	$OpenBSD: parent.c,v 1.1 2026/06/11 15:41:33 bluhm Exp $	*/

/*
 * Copyright (c) 2026 Alexander Bluhm <bluhm@openbsd.org>
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

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "log.h"
#include "syslogd.h"

static int	 verbose;
static char	*debug_ebuf;

/* parent process is re-execed with reduced arguments, others ignored */

static __dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: syslogd-parent [-dn] [-f config_file] -P child_pid\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *ConfFile = _PATH_LOGCONF;
	int Debug = 0;
	int PrivChild = 0;
	int NoDNS = 0;
	const char *errstr;
	int ch;

	while ((ch = getopt(argc, argv,
	    "46a:C:c:dFf:hK:k:m:nP:p:rS:s:T:U:uVZ")) != -1) {
		switch (ch) {
		case '4':
		case '6':
		case 'a':
		case 'C':
		case 'c':
		case 'd':		/* debug */
			Debug++;
			break;
		case 'F':
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'h':
		case 'K':
		case 'k':
		case 'm':
			break;
		case 'n':		/* don't do DNS lookups */
			NoDNS = 1;
			break;
		case 'P':		/* used internally, exec the parent */
			PrivChild = strtonum(optarg, 2, INT_MAX, &errstr);
			if (errstr)
				errx(1, "priv child %s: %s", errstr, optarg);
			break;
		case 'p':
		case 'r':
		case 'S':
		case 's':
		case 'T':
		case 'U':
		case 'u':
		case 'V':
		case 'Z':
			break;
		default:
			usage();
		}
	}
	if (argc != optind)
		usage();
	if (PrivChild < 2)
		errx(1, "parent requires -P child_pid");

	log_init(Debug, LOG_SYSLOG);
	priv_exec(ConfFile, NoDNS, PrivChild, argc, argv);

	/* NOTREACHED */
	return 1;
}

void
log_init(int n_debug, int fac)
{
	verbose = n_debug;

	if (debug_ebuf == NULL)
		if ((debug_ebuf = malloc(ERRBUFSIZE)) == NULL)
		    err(1, "allocate debug buffer");
	debug_ebuf[0] = '\0';
}

void
log_debug(const char *emsg, ...)
{
	va_list ap;
	int saved_errno;

	if (verbose) {
		saved_errno = errno;
		va_start(ap, emsg);
		vsnprintf(debug_ebuf, ERRBUFSIZE, emsg, ap);
		fprintf(stderr, "[priv]: %s\n", debug_ebuf);
		fflush(stderr);
		va_end(ap);
		errno = saved_errno;
	}
	debug_ebuf[0] = '\0';
}

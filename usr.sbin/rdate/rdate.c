/*	$OpenBSD: rdate.c,v 1.17 2002/07/27 20:11:34 jakob Exp $	*/
/*	$NetBSD: rdate.c,v 1.4 1996/03/16 12:37:45 pk Exp $	*/

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rdate.c: Set the date from the specified host
 *
 *	Uses the rfc868 time protocol at socket 37.
 *	Time is returned as the number of seconds since
 *	midnight January 1st 1900.
 */
#ifndef lint
#if 0
from: static char rcsid[] = "$NetBSD: rdate.c,v 1.3 1996/02/22 06:59:18 thorpej Exp $";
#else
static const char rcsid[] = "$OpenBSD: rdate.c,v 1.17 2002/07/27 20:11:34 jakob Exp $";
#endif
#endif				/* lint */

#include <sys/types.h>
#include <sys/param.h>
#include <stdio.h>
#include <ctype.h>
#include <err.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <util.h>
#include <time.h>

void rfc868time_client (const char *, struct timeval *, struct timeval *);
void ntp_client (const char *, struct timeval *, struct timeval *);

extern char    *__progname;
extern int	corrleaps;

void
usage()
{
	(void) fprintf(stderr, "Usage: %s [-npsa] host\n", __progname);
	(void) fprintf(stderr, "  -n: use SNTP instead of RFC868 time protocol\n");
	(void) fprintf(stderr, "  -N: use SNTP and correct leap seconds\n");
	(void) fprintf(stderr, "  -p: just print, don't set\n");
	(void) fprintf(stderr, "  -s: just set, don't print\n");
	(void) fprintf(stderr, "  -a: use adjtime instead of instant change\n");
	(void) fprintf(stderr, "  -v: verbose output\n");
}

int
main(int argc, char **argv)
{
	int             pr = 0, silent = 0, ntp = 0, verbose = 0;
	int		slidetime = 0;
	char           *hname;
	extern int      optind;
	int             c;

	struct timeval new, adjust;

	while ((c = getopt(argc, argv, "psanNv")) != -1)
		switch (c) {
		case 'p':
			pr++;
			break;

		case 's':
			silent++;
			break;

		case 'a':
			slidetime++;
			break;

		case 'n':
			ntp++;
			corrleaps = 0;
			break;

		case 'N':
			ntp++;
			corrleaps = 1;
			break;

		case 'v':
			verbose++;
			break;

		default:
			usage();
			return 1;
		}

	if (argc - 1 != optind) {
		usage();
		return 1;
	}
	hname = argv[optind];

	if (ntp)
		ntp_client(hname, &new, &adjust);
	else
		rfc868time_client(hname, &new, &adjust);

	if (!pr) {
		if (!slidetime) {
			logwtmp("|", "date", "");
			if (settimeofday(&new, NULL) == -1)
				err(1, "Could not set time of day");
			logwtmp("{", "date", "");
		} else {
			if (adjtime(&adjust, NULL) == -1)
				err(1, "Could not adjust time of day");
		}
	}

	if (!silent) {
		struct tm      *ltm;
		char		buf[80];
		time_t		tim = new.tv_sec;
		double		adjsec;

		ltm = localtime(&tim);
		(void) strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Z %Y\n", ltm);
		(void) fputs(buf, stdout);

		adjsec  = adjust.tv_sec + adjust.tv_usec / 1.0e6;

		if (slidetime || verbose) {
			if (ntp)
				(void) fprintf(stdout,
				   "%s: adjust local clock by %.6f seconds\n",
				   __progname, adjsec);
			else
				(void) fprintf(stdout,
				   "%s: adjust local clock by %ld seconds\n",
				   __progname, adjust.tv_sec);
		}
	}

	return 0;
}

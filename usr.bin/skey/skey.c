/* * $OpenBSD: skey.c,v 1.2 1996/06/26 05:39:20 deraadt Exp $*/
/*
 * S/KEY v1.1b (skey.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *
 *
 * Stand-alone program for computing responses to S/Key challenges.
 * Takes the iteration count and seed as command line args, prompts
 * for the user's key, and produces both word and hex format responses.
 *
 * Usage example:
 *	>skey 88 ka9q2
 *	Enter password:
 *	OMEN US HORN OMIT BACK AHOY
 *	>
 *
 */

#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sgtty.h>
#include "md4.h"
#include "skey.h"

void    usage __P((char *));

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int     n, cnt, i, pass = 0;
	char    passwd[256], key[8], buf[33], *seed, *slash;
	extern int optind;
	extern char *optarg;

	cnt = 1;

	while ((i = getopt(argc, argv, "n:p:")) != EOF) {
		switch (i) {
		case 'n':
			cnt = atoi(optarg);
			break;
		case 'p':
			strcpy(passwd, optarg);
			pass = 1;
			break;
		}
	}

	/* could be in the form <number>/<seed> */

	if (argc <= optind + 1) {
		/* look for / in it */
		if (argc <= optind)
			usage(argv[0]);
		slash = strchr(argv[optind], '/');
		if (slash == NULL)
			usage(argv[0]);
		*slash++ = '\0';
		seed = slash;

		if ((n = atoi(argv[optind])) < 0) {
			fprintf(stderr, "%s not positive\n", argv[optind]);
			usage(argv[0]);
		}
	} else {

		if ((n = atoi(argv[optind])) < 0) {
			fprintf(stderr, "%s not positive\n", argv[optind]);
			usage(argv[0]);
		}
		seed = argv[++optind];
	}

	/* Get user's secret password */
	if (!pass) {
		fprintf(stderr, "Enter secret password: ");
		readpass(passwd, sizeof(passwd));
	}
	rip(passwd);

	/* Crunch seed and password into starting key */
	if (keycrunch(key, seed, passwd) != 0) {
		fprintf(stderr, "%s: key crunch failed\n", argv[0]);
		exit(1);
	}
	if (cnt == 1) {
		while (n-- != 0)
			f(key);
		printf("%s\n", btoe(buf, key));
#ifdef	HEXIN
		printf("%s\n", put8(buf, key));
#endif
	} else {
		for (i = 0; i <= n - cnt; i++)
			f(key);
		for (; i <= n; i++) {
#ifdef	HEXIN
			printf("%d: %-29s  %s\n", i, btoe(buf, key), put8(buf, key));
#else
			printf("%d: %-29s\n", i, btoe(buf, key));
#endif
			f(key);
		}
	}
	exit(0);
}

void
usage(s)
	char   *s;
{

	fprintf(stderr,
	    "Usage: %s [-n count] [-p password ] sequence# [/] key\n", s);
	exit(1);
}

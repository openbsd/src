/* * $OpenBSD: skey.c,v 1.12 1999/03/06 20:27:41 millert Exp $*/
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <skey.h>

void    usage __P((char *));

int
main(argc, argv)
	int	argc;
	char	*argv[];
{
	int     n, i, cnt = 1, pass = 0, hexmode = 0;
	char    passwd[SKEY_MAX_PW_LEN+1], key[SKEY_BINKEY_SIZE];
	char	buf[33], *seed, *slash;

	/* If we were called as otp-METHOD, set algorithm based on that */
	if ((slash = strrchr(argv[0], '/')))
	    slash++;
	else
	    slash = argv[0];
	if (strncmp(slash, "otp-", 4) == 0) {
		slash += 4;
		if (skey_set_algorithm(slash) == NULL)
			errx(1, "Unknown hash algorithm %s", slash);
	}

	for (i = 1; i < argc && argv[i][0] == '-' && strcmp(argv[i], "--");) {
		if (argv[i][2] == '\0') {
			/* Single character switch */
			switch (argv[i][1]) {
			case 'n':
				if (i + 1 == argc)
					usage(argv[0]);
				cnt = atoi(argv[++i]);
				break;
			case 'p':
				if (i + 1 == argc)
					usage(argv[0]);
				(void)strcpy(passwd, argv[++i]);
				pass = 1;
				break;
			case 'x':
				hexmode = 1;
				break;
			default:
				usage(argv[0]);
			}
		} else {
			/* Multi character switches are hash types */
			if (skey_set_algorithm(&argv[i][1]) == NULL) {
				warnx("Unknown hash algorithm %s", &argv[i][1]);
				usage(argv[0]);
			}
		}
		i++;
	}

	if (argc > i + 2)
		usage(argv[0]);

	/* Could be in the form <number>/<seed> */
	if (argc <= i + 1) {
		/* look for / in it */
		if (argc <= i)
			usage(argv[0]);
		slash = strchr(argv[i], '/');
		if (slash == NULL)
			usage(argv[0]);
		*slash++ = '\0';
		seed = slash;

		if ((n = atoi(argv[i])) < 0) {
			warnx("%d not positive", n);
			usage(argv[0]);
		} else if (n > SKEY_MAX_SEQ) {
			warnx("%d is larger than max (%d)", n, SKEY_MAX_SEQ);
			usage(argv[0]);
		}
	} else {
		if ((n = atoi(argv[i])) < 0) {
			warnx("%d not positive", n);
			usage(argv[0]);
		} else if (n > SKEY_MAX_SEQ) {
			warnx("%d is larger than max (%d)", n, SKEY_MAX_SEQ);
			usage(argv[0]);
		}
		seed = argv[++i];
	}

	/* Get user's secret password */
	if (!pass) {
		(void)fputs("Reminder - Do not use this program while logged in via telnet or rlogin.\n", stderr);
		(void)fputs("Enter secret password: ", stderr);
		readpass(passwd, sizeof(passwd));
		if (passwd[0] == '\0') 
			exit(1);
	}

	/* Crunch seed and password into starting key */
	if (keycrunch(key, seed, passwd) != 0)
		errx(1, "key crunch failed");

	if (cnt == 1) {
		while (n-- != 0)
			f(key);
		(void)puts(hexmode ? put8(buf, key) : btoe(buf, key));
	} else {
		for (i = 0; i <= n - cnt; i++)
			f(key);
		for (; i <= n; i++) {
			if (hexmode)
				(void)printf("%d: %-29s  %s\n", i,
				    btoe(buf, key), put8(buf, key));
			else
				(void)printf("%d: %-29s\n", i, btoe(buf, key));
			f(key);
		}
	}
	exit(0);
}

void
usage(s)
	char   *s;
{
	(void)fprintf(stderr, "Usage: %s [-x] [-md4|-md5|-sha1|-rmd160] [-n count] [-p password] <sequence#>[/] key\n", s);
	exit(1);
}

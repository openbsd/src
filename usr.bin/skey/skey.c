/*	$OpenBSD: skey.c,v 1.31 2015/10/09 21:59:34 tim Exp $	*/
/*
 * OpenBSD S/Key (skey.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <Todd.Miller@courtesan.com>
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

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readpassphrase.h>
#include <skey.h>

void usage();

extern char *__progname;

int
main(int argc, char *argv[])
{
	int     n, i, cnt = 1, pass = 0, hexmode = 0;
	char    passwd[SKEY_MAX_PW_LEN+1], key[SKEY_BINKEY_SIZE];
	char	buf[33], *seed, *slash, *algo;
	const char *errstr;

	/* If we were called as otp-METHOD, set algorithm based on that */
	if (strncmp(__progname, "otp-", 4) == 0) {
		algo = __progname + 4;
		if (skey_set_algorithm(algo) == NULL)
			errx(1, "Unknown hash algorithm %s", algo);
	}

	for (i = 1; i < argc && argv[i][0] == '-' && strcmp(argv[i], "--");) {
		if (argv[i][2] == '\0') {
			/* Single character switch */
			switch (argv[i][1]) {
			case 'n':
				if (++i == argc)
					usage();
				cnt = strtonum(argv[i], 1, SKEY_MAX_SEQ -1, &errstr);
				if (errstr)
					usage();
				break;
			case 'p':
				if (++i == argc)
					usage();
				if (strlcpy(passwd, argv[i], sizeof(passwd)) >=
				    sizeof(passwd))
					errx(1, "Password too long");
				pass = 1;
				break;
			case 'x':
				hexmode = 1;
				break;
			default:
				usage();
			}
		} else {
			/* Multi character switches are hash types */
			if (skey_set_algorithm(&argv[i][1]) == NULL) {
				warnx("Unknown hash algorithm %s", &argv[i][1]);
				usage();
			}
		}
		i++;
	}

	if (argc > i + 2)
		usage();

	/* Could be in the form <number>/<seed> */
	if (argc <= i + 1) {
		/* look for / in it */
		if (argc <= i)
			usage();
		slash = strchr(argv[i], '/');
		if (slash == NULL)
			usage();
		*slash++ = '\0';
		seed = slash;

		n = strtonum(argv[i], 0, SKEY_MAX_SEQ, &errstr);
		if (errstr) {
			warnx("%s: %s", argv[i], errstr);
			usage();
		}
	} else {
		n = strtonum(argv[i], 0, SKEY_MAX_SEQ, &errstr);
		if (errstr) {
			warnx("%s: %s", argv[i], errstr);
			usage();
		}
		seed = argv[++i];
	}

	/* Get user's secret passphrase */
	if (!pass && (readpassphrase("Enter secret passphrase: ", passwd,
	    sizeof(passwd), 0) == NULL || passwd[0] == '\0'))
		exit(1);

	/* Crunch seed and passphrase into starting key */
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
				(void)printf("%d: %s\n", i, put8(buf, key));
			else
				(void)printf("%d: %-29s\n", i, btoe(buf, key));
			f(key);
		}
	}
	exit(0);
}

void
usage(void)
{
	fprintf(stderr,
	    "usage: %s [-x] [-md5 | -rmd160 | -sha1] [-n count]\n\t"
	    "[-p passphrase] <sequence#>[/] key\n", __progname);
	exit(1);
}

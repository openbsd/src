/*	$OpenBSD: skeyinit.c,v 1.5 1996/09/28 00:04:44 millert Exp $	*/
/*	$NetBSD: skeyinit.c,v 1.6 1995/06/05 19:50:48 pk Exp $	*/

/* S/KEY v1.1b (skeyinit.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *
 * S/KEY initialization and seed update
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>
#include <skey.h>

#ifndef SKEY_MAXSEQ
#define SKEY_MAXSEQ	10000
#endif
#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN	4
#endif
#ifndef SKEY_MIN_PW_LEN
#define SKEY_MIN_PW_LEN	4
#endif

int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     rval, n, nn, i, l, md=0, defaultsetup=1, zerokey=0, hexmode=0;
	time_t  now;
	char	hostname[MAXHOSTNAMELEN];
	char    seed[18], tmp[80], key[8], defaultseed[17];
	char    passwd[256], passwd2[256], tbuf[27], buf[60];
	char    lastc, me[80], *salt, *p, *pw;
	struct skey skey;
	struct passwd *pp;
	struct tm *tm;

	if (geteuid() != 0)
		errx(1, "must be setuid root.");

	(void)time(&now);
	(void)sprintf(tbuf, "%05ld", (long) (now % 100000));

	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	(void)strncpy(defaultseed, hostname, sizeof(defaultseed) - 1);
	defaultseed[SKEY_NAMELEN] = '\0';
	(void)strncat(defaultseed, tbuf, sizeof(defaultseed) - 5);

	if ((pp = getpwuid(getuid())) == NULL)
		err(1, "no user with uid %d", getuid());
	(void)strcpy(me, pp->pw_name);

	if ((pp = getpwnam(me)) == NULL)
		err(1, "Who are you?");

	while ((i = getopt(argc, argv, "sxz45")) != EOF) {
		switch (i) {
			case 's':
				defaultsetup = 0;
				break;
			case 'x':
				hexmode = 1;
				break;
			case 'z':
				zerokey = 1;
				break;
			case '4':
				md = 4;
				break;
			case '5':
				md = 5;
				break;
		}
	}
	if (argc - optind  > 1) {
		(void)fprintf(stderr,
			"Usage: %s [-s] [-x] [-z] [-4|-5] [user]\n", argv[0]);
		exit(1);
	} else if (argv[optind]) {
		if ((pp = getpwnam(argv[optind])) == NULL)
			err(1, "User unknown");

		if (strcmp(pp->pw_name, me) != 0) {
			if (getuid() != 0) {
				/* Only root can change other's passwds */
				errx(1, "Permission denied.");
			}
		}
	}
	salt = pp->pw_passwd;

	(void)setpriority(PRIO_PROCESS, 0, -4);

	if (getuid() != 0) {
		(void)setpriority(PRIO_PROCESS, 0, -4);

		pw = getpass("Password:");
		p = crypt(pw, salt);

		(void)setpriority(PRIO_PROCESS, 0, 0);

		if (pp && strcmp(p, pp->pw_passwd))
			errx(1, "Password incorrect.");
	}

	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
		case -1:
			err(1, "cannot open database");
		case 0:
			/* comment out user if asked to */
			if (zerokey)
				exit(skeyzero(&skey, pp->pw_name));

			(void)printf("[Updating %s]\n", pp->pw_name);
			(void)printf("Old key: %s\n", skey.seed);

			/*
			 * Lets be nice if they have a skey.seed that
			 * ends in 0-8 just add one
			 */
			l = strlen(skey.seed);
			if (l > 0) {
				lastc = skey.seed[l - 1];
				if (isdigit(lastc) && lastc != '9') {
					(void)strcpy(defaultseed, skey.seed);
					defaultseed[l - 1] = lastc + 1;
				}
				if (isdigit(lastc) && lastc == '9' && l < 16) {
					(void)strcpy(defaultseed, skey.seed);
					defaultseed[l - 1] = '0';
					defaultseed[l] = '0';
					defaultseed[l + 1] = '\0';
				}
			}
			break;
		case 1:
			if (zerokey)
				errx(1, "You have no entry to zero.");
			(void)printf("[Adding %s]\n", pp->pw_name);
			break;
	}
	n = 99;

	/* Set MDX (currently 4 or 5) if given the option */
	if (md)
		skey_set_MDX(md);

	if (!defaultsetup) {
		(void)printf("You need the 6 english words generated from the \"skey\" command.\n");
		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);
			(void)printf("Enter sequence count from 1 to %d: ",
				     SKEY_MAXSEQ);
			(void)fgets(tmp, sizeof(tmp), stdin);
			n = atoi(tmp);
			if (n > 0 && n < SKEY_MAXSEQ)
				break;	/* Valid range */
			(void)printf("\n Error: Count must be > 0 and < %d\n",
				     SKEY_MAXSEQ);
		}

		(void)printf("Enter new key [default %s]: ", defaultseed);
		(void)fflush(stdout);
		(void)fgets(seed, sizeof(seed), stdin);
		rip(seed);
		if (strlen(seed) > 16) {
			(void)puts("Notice: Seed truncated to 16 characters.");
			seed[16] = '\0';
		}
		if (seed[0] == '\0')
			(void)strcpy(seed, defaultseed);

		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			(void)printf("s/key %d %s\ns/key access password: ",
			    n, seed);
			(void)fgets(tmp, sizeof(tmp), stdin);
			rip(tmp);
			backspace(tmp);

			if (tmp[0] == '?') {
				(void)puts("Enter 6 English words from secure S/Key calculation.");
				continue;
			} else if (tmp[0] == '\0')
				exit(1);
			if (etob(key, tmp) == 1 || atob8(key, tmp) == 0)
				break;	/* Valid format */
			(void)puts("Invalid format - try again with 6 English words.");
		}
	} else {
		/* Get user's secret password */
		fputs("Reminder - Only use this method if you are directly connected\n           or have an encrypted channel.  If you are using telnet\n           or rlogin, exit with no password and use keyinit -s.\n", stderr);

		for (i = 0;; i++) {
			if (i > 2)
				exit(1);

			(void)fputs("Enter secret password: ", stderr);
			readpass(passwd, sizeof(passwd));
			if (passwd[0] == '\0')
				exit(1);

			if (strlen(passwd) < SKEY_MIN_PW_LEN) {
				(void)fputs("Your password must be longer.\n",
					    stderr);
				continue;
			}

			(void)fputs("Again secret password: ", stderr);
			readpass(passwd2, sizeof(passwd));
			if (passwd2[0] == '\0')
				exit(1);

			if (strcmp(passwd, passwd2) == 0)
				break;

			(void)fputs("Passwords do not match.\n", stderr);
		}

		/* Crunch seed and password into starting key */
		(void)strcpy(seed, defaultseed);
		if (keycrunch(key, seed, passwd) != 0)
			err(2, "key crunch failed");

		nn = n;
		while (nn-- != 0)
			f(key);
	}
	(void)time(&now);
	tm = localtime(&now);
	(void)strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	if ((skey.val = (char *)malloc(16 + 1)) == NULL)
		err(1, "Can't allocate memory");

	btoa8(skey.val, key);

	(void)fprintf(skey.keyfile, "%s MD%d %04d %-16s %s %-21s\n",
	    pp->pw_name, skey_get_MDX(), n, seed, skey.val, tbuf);
	(void)fclose(skey.keyfile);
	(void)printf("\nID %s s/key is %d %s\n", pp->pw_name, n, seed);
	(void)printf("Next login password: %s\n", hexmode ? put8(buf, key) : btoe(buf, key));

	exit(0);
}

/*	$OpenBSD: skeyinit.c,v 1.12 1996/10/02 03:49:34 millert Exp $	*/
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
#include <utmp.h>
#include <ctype.h>
#include <skey.h>

#ifndef SKEY_MAXSEQ
#define SKEY_MAXSEQ	10000
#endif
#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN	4
#endif
#ifndef SKEY_MIN_PW_LEN
#define SKEY_MIN_PW_LEN	10
#endif

void	usage __P((char *));

int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     rval, n, nn, i, l, defaultsetup=1, zerokey=0, hexmode=0;
	time_t  now;
	char	hostname[MAXHOSTNAMELEN];
	char    seed[18], tmp[80], key[8], defaultseed[17];
	char    passwd[256], passwd2[256], tbuf[27], buf[60];
	char    lastc, me[UT_NAMESIZE+1], *salt, *p, *pw, *ht=NULL;
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

	for (i = 1; i < argc && argv[i][0] == '-' && strcmp(argv[i], "--");) {
		if (argv[i][2] == '\0') {
			/* Single character switch */
			switch (argv[i][1]) {
			case 's':
				defaultsetup = 0;
				break;
			case 'x':
				hexmode = 1;
				break;
			case 'z':
				zerokey = 1;
				break;
			default:
				usage(argv[0]);
			}
		} else {
			/* Multi character switches are hash types */
			if ((ht = skey_set_algorithm(&argv[i][1])) == NULL) {
				warnx("Unknown hash algorithm %s", &argv[i][1]);
				usage(argv[0]);
			}
		}
		i++;
	}

	/* check for optional user string */
	if (argc - i  > 1) {
		usage(argv[0]);
	} else if (argv[i]) {
		if ((pp = getpwnam(argv[i])) == NULL)
			err(1, "User unknown");

		if (strcmp(pp->pw_name, me) != 0) {
			if (getuid() != 0) {
				/* Only root can change other's passwds */
				errx(1, "Permission denied.");
			}
		}
	}
	salt = pp->pw_passwd;

	if (getuid() != 0) {
		pw = getpass("Password (or `s/key'):");
		if (strcasecmp(pw, "s/key") == 0) {
			if (skey_haskey(me))
				exit(1);
			if (skey_authenticate(me))
				errx(1, "Password incorrect.");
		} else {
			p = crypt(pw, salt);
			if (strcmp(p, pp->pw_passwd))
				errx(1, "Password incorrect.");
		}
	}

	(void)setpriority(PRIO_PROCESS, 0, -4);

	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
		case -1:
			err(1, "cannot open database");
		case 0:
			/* comment out user if asked to */
			if (zerokey)
				exit(skeyzero(&skey, pp->pw_name));

			(void)printf("[Updating %s]\n", pp->pw_name);
			(void)printf("Old key: [%s] %s\n", skey_get_algorithm(),
				     skey.seed);

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

	/* Set hash type if asked to */
	if (ht)
		skey_set_algorithm(ht);

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
			(void)printf("Error: Count must be > 0 and < %d\n",
				     SKEY_MAXSEQ);
		}

		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			(void)printf("Enter new key [default %s]: ",
				     defaultseed);
			(void)fgets(seed, sizeof(seed), stdin);
			rip(seed);
			for (p = seed; *p; p++) {
				if (isalpha(*p)) {
					if (isupper(*p))
						*p = tolower(*p);
				} else if (!isdigit(*p)) {
					(void)puts("Error: seed may only contain alpha numeric characters");
					break;
				}
			}
			if (*p == '\0')
				break;  /* Valid seed */
		}
		if (strlen(seed) > 16) {
			(void)puts("Notice: Seed truncated to 16 characters.");
			seed[16] = '\0';
		}
		if (seed[0] == '\0')
			(void)strcpy(seed, defaultseed);

		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			(void)printf("otp-%s %d %s\nS/Key access password: ",
				     skey_get_algorithm(), n, seed);
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

	/* Don't save algorithm type for md4 (keep record length same) */
	if (strcmp(skey_get_algorithm(), "md4") == 0)
		(void)fprintf(skey.keyfile, "%s s %04d %-16s %s %-21s\n",
		    pp->pw_name, n, seed, skey.val, tbuf);
	else
		(void)fprintf(skey.keyfile, "%s %s %04d %-16s %s %-21s\n",
		    pp->pw_name, skey_get_algorithm(), n, seed, skey.val, tbuf);
	(void)fclose(skey.keyfile);

	(void)setpriority(PRIO_PROCESS, 0, 0);

	(void)printf("\nID %s skey is otp-%s %d %s\n", pp->pw_name,
		     skey_get_algorithm(), n, seed);
	(void)printf("Next login password: %s\n\n",
		     hexmode ? put8(buf, key) : btoe(buf, key));
	exit(0);
}

void
usage(s)
	char *s;
{
	(void)fprintf(stderr,
		"Usage: %s [-s] [-x] [-z] [-md4|-md5|-sha1] [user]\n", s);
	exit(1);
}

/*	$OpenBSD: skeyinit.c,v 1.3 1996/06/26 05:39:24 deraadt Exp $	*/
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

#include "skey.h"

#define NAMELEN 2

int skeylookup __ARGS((struct skey * mp, char *name));
int skeyzero __ARGS((struct skey * mp, char *name));

int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     rval, n, nn, i, defaultsetup, l, zerokey = 0;
	time_t  now;
	char	hostname[MAXHOSTNAMELEN];
	char    seed[18], tmp[80], key[8], defaultseed[17];
	char    passwd[256], passwd2[256], tbuf[27], buf[60];
	char    lastc, me[80], user[8], *salt, *p, *pw;
	struct skey skey;
	struct passwd *pp;
	struct tm *tm;

	time(&now);
	tm = localtime(&now);
	strftime(tbuf, sizeof(tbuf), "%M%j", tm);

	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	strncpy(defaultseed, hostname, sizeof(defaultseed)- 1);
	defaultseed[4] = '\0';
	strncat(defaultseed, tbuf, sizeof(defaultseed) - 5);

	if ((pp = getpwuid(getuid())) == NULL)
		err(1, "no user with uid %d", getuid());
	strcpy(me, pp->pw_name);

	if ((pp = getpwnam(me)) == NULL)
		err(1, "Who are you?");

	defaultsetup = 1;
	for (i=1; i < argc; i++) {
		if (strcmp("-s", argv[i]) == 0)
			defaultsetup = 0;
		else if (strcmp("-z", argv[i]) == 0)
			zerokey = 1;
		else {
			pp = getpwnam(argv[i]);
			break;
		}
	}
	if (pp == NULL) {
		err(1, "User unknown");
	}
	if (strcmp(pp->pw_name, me) != 0) {
		if (getuid() != 0) {
			/* Only root can change other's passwds */
			printf("Permission denied.\n");
			exit(1);
		}
	}
	salt = pp->pw_passwd;

	setpriority(PRIO_PROCESS, 0, -4);

	if (getuid() != 0) {
		setpriority(PRIO_PROCESS, 0, -4);

		pw = getpass("Password:");
		p = crypt(pw, salt);

		setpriority(PRIO_PROCESS, 0, 0);

		if (pp && strcmp(p, pp->pw_passwd)) {
			printf("Password incorrect.\n");
			exit(1);
		}
	}
	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
	case -1:
		err(1, "cannot open database");
	case 0:
		/* comment out user if asked to */
		if (zerokey)
			exit(skeyzero(&skey, pp->pw_name));

		printf("[Updating %s]\n", pp->pw_name);
		printf("Old key: %s\n", skey.seed);

		/*
		 * lets be nice if they have a skey.seed that
		 * ends in 0-8 just add one
		 */
		l = strlen(skey.seed);
		if (l > 0) {
			lastc = skey.seed[l - 1];
			if (isdigit(lastc) && lastc != '9') {
				strcpy(defaultseed, skey.seed);
				defaultseed[l - 1] = lastc + 1;
			}
			if (isdigit(lastc) && lastc == '9' && l < 16) {
				strcpy(defaultseed, skey.seed);
				defaultseed[l - 1] = '0';
				defaultseed[l] = '0';
				defaultseed[l + 1] = '\0';
			}
		}
		break;
	case 1:
		if (zerokey) {
			printf("You have no entry to zero.\n");
			exit(1);
		}
		printf("[Adding %s]\n", pp->pw_name);
		break;
	}
	n = 99;

	if (!defaultsetup) {
		printf("You need the 6 english words generated from the \"key\" command.\n");
		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);
			printf("Enter sequence count from 1 to 10000: ");
			fgets(tmp, sizeof(tmp), stdin);
			n = atoi(tmp);
			if (n > 0 && n < 10000)
				break;	/* Valid range */
			printf("\n Error: Count must be > 0 and < 10000\n");
		}
	}
	if (!defaultsetup) {
		printf("Enter new key [default %s]: ", defaultseed);
		fflush(stdout);
		fgets(seed, sizeof(seed), stdin);
		rip(seed);
		if (strlen(seed) > 16) {
			printf("Notice: Seed truncated to 16 characters.\n");
			seed[16] = '\0';
		}
		if (seed[0] == '\0')
			strcpy(seed, defaultseed);

		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			printf("s/key %d %s\ns/key access password: ", n, seed);
			fgets(tmp, sizeof(tmp), stdin);
			rip(tmp);
			backspace(tmp);

			if (tmp[0] == '?') {
				printf("Enter 6 English words from secure S/Key calculation.\n");
				continue;
			}
			if (tmp[0] == '\0') {
				exit(1);
			}
			if (etob(key, tmp) == 1 || atob8(key, tmp) == 0)
				break;	/* Valid format */
			printf("Invalid format - try again with 6 English words.\n");
		}
	} else {
		/* Get user's secret password */
		for (i = 0;; i++) {
			if (i >= 2)
				exit(1);

			printf("Enter secret password: ");
			readpass(passwd, sizeof(passwd));
			if (passwd[0] == '\0')
				exit(1);

			printf("Again secret password: ");
			readpass(passwd2, sizeof(passwd));
			if (passwd2[0] == '\0')
				exit(1);

			if (strlen(passwd) < 4 && strlen(passwd2) < 4)
				err(1, "Your password must be longer");
			if (strcmp(passwd, passwd2) == 0)
				break;

			printf("Passwords do not match.\n");
		}
		strcpy(seed, defaultseed);

		/* Crunch seed and password into starting key */
		if (keycrunch(key, seed, passwd) != 0)
			err(2, "key crunch failed");
		nn = n;
		while (nn-- != 0)
			f(key);
	}
	time(&now);
	tm = localtime(&now);
	strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	skey.val = (char *)malloc(16 + 1);

	btoa8(skey.val, key);

	fprintf(skey.keyfile, "%s %04d %-16s %s %-21s\n", pp->pw_name, n,
	    seed, skey.val, tbuf);
	fclose(skey.keyfile);
	printf("ID %s s/key is %d %s\n", pp->pw_name, n, seed);
	printf("Next login password: %s\n", btoe(buf, key));
#ifdef HEXIN
	printf("%s\n", put8(buf, key));
#endif

	exit(1);
}

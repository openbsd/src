/*	$OpenBSD: skeyinit.c,v 1.30 2001/11/01 18:26:58 miod Exp $	*/

/* OpenBSD S/Key (skeyinit.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * S/Key initialization and seed update
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <err.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <utmp.h>

#include <skey.h>

#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN    4
#endif

void	lockeof __P((struct skey *, char *));
void	usage __P((char *));
void	secure_mode __P((int *, char *, char *, char *, char *, size_t));
void	normal_mode __P((char *, int, char *, char *, char *));
void	timedout __P((int));

int
main(argc, argv)
	int     argc;
	char   *argv[];
{
	int     rval, i, l, n=0, defaultsetup=1, zerokey=0, hexmode=0;
	int	oldmd4=0;
	time_t  now;
	size_t	seedlen;
	char	hostname[MAXHOSTNAMELEN];
	char    passwd[SKEY_MAX_PW_LEN+2];
	char	seed[SKEY_MAX_SEED_LEN+2], defaultseed[SKEY_MAX_SEED_LEN+1];
	char    tbuf[27], buf[256], key[SKEY_BINKEY_SIZE];
	char    lastc, me[UT_NAMESIZE+1], *salt, *p, *ht=NULL;
	struct skey skey;
	struct passwd *pp;
	struct tm *tm;

	if (geteuid() != 0)
		errx(1, "must be setuid root.");

	/* Build up a default seed based on the hostname and time */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	for (i = 0, p = defaultseed; hostname[i] && i < SKEY_NAMELEN; i++) {
		if (isalpha(hostname[i])) {
			if (isupper(hostname[i]))
				hostname[i] = tolower(hostname[i]);
			*p++ = hostname[i];
		} else if (isdigit(hostname[i]))
			*p++ = hostname[i];
	}
	*p = '\0';
	(void)time(&now);
	(void)sprintf(tbuf, "%05ld", (long) (now % 100000));
	(void)strncat(defaultseed, tbuf, sizeof(defaultseed) - 5);

	if ((pp = getpwuid(getuid())) == NULL)
		err(1, "no user with uid %d", getuid());
	(void)strcpy(me, pp->pw_name);

	if ((pp = getpwnam(me)) == NULL)
		err(1, "Who are you?");
	salt = pp->pw_passwd;

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
			case 'n':
				if (argv[++i] == NULL || argv[i][0] == '\0')
					usage(argv[0]);
				if ((n = atoi(argv[i])) < 1 || n >= SKEY_MAX_SEQ)
					errx(1, "count must be > 0 and < %d",
					     SKEY_MAX_SEQ);
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
		if ((pp = getpwnam(argv[i])) == NULL) {
			if (getuid() == 0) {
				static struct passwd _pp;

				_pp.pw_name = argv[i];
				pp = &_pp;
				warnx("Warning, user unknown: %s", argv[i]);
			} else {
				errx(1, "User unknown: %s", argv[i]);
			}
		} else if (strcmp(pp->pw_name, me) != 0) {
			if (getuid() != 0) {
				/* Only root can change other's passwds */
				errx(1, "Permission denied.");
			}
		}
	}

	if (defaultsetup)
		fputs("Reminder - Only use this method if you are directly connected\n           or have an encrypted channel.  If you are using telnet\n           or rlogin, hit return now and use skeyinit -s.\n", stderr);

	if (getuid() != 0) {
		/* XXX - use BSD auth */
		passwd[0] = '\0';
		if (!defaultsetup && skeychallenge(&skey, me, buf) == 0) {
			printf("Enter S/Key password below or hit return twice "
			    "to enter standard password.\n%s\n", buf);
			fflush(stdout);
			if (!readpassphrase("S/Key Password: ", passwd,
			    sizeof(passwd), 0) || passwd[0] == '\0') {
				readpassphrase("S/Key Password: [echo on] ",
				    passwd, sizeof(passwd), RPP_ECHO_ON);
			}
		}
		if (passwd[0]) {
			if (skeyverify(&skey, passwd) != 0)
				errx(1, "Password incorrect.");
		} else {
			fflush(stdout);
			readpassphrase("Password: ", passwd, sizeof(passwd), 0);
			if (strcmp(crypt(passwd, salt), pp->pw_passwd)) {
				if (passwd[0])
					warnx("Password incorrect.");
				exit(1);
			}
		}
	}

	/*
	 * Lookup and lock the record we are about to modify.
	 * If this is a new entry this will prevent other users
	 * from appending new entries (and clobbering ours).
	 */
	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
		case -1:
			if (errno == ENOENT)
				errx(1, "S/Key disabled");
			else
				err(1, "cannot open database");
			break;
		case 0:
			/* comment out user if asked to */
			if (zerokey)
				exit(skeyzero(&skey));

			(void)printf("[Updating %s with %s]\n", pp->pw_name,
			    ht ? ht : skey_get_algorithm());
			(void)printf("Old seed: [%s] %s\n",
				     skey_get_algorithm(), skey.seed);

			/*
			 * Sanity check old seed.
			 */
			l = strlen(skey.seed);
			for (p = skey.seed; *p; p++) {
				if (isalpha(*p)) {
					if (isupper(*p))
						*p = tolower(*p);
				} else if (!isdigit(*p)) {
					memmove(p, p + 1, l - (p - skey.seed));
					l--;
				}
			}

			/* If the seed ends in 0-8 just add one.  */
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
			(void)printf("[Adding %s with %s]\n", pp->pw_name,
			    ht ? ht : skey_get_algorithm());
			lockeof(&skey, pp->pw_name);
			break;
	}
	if (n == 0)
		n = 99;

	/* Do we have an old-style md4 entry? */
	if (rval == 0 && strcmp("md4", skey_get_algorithm()) == 0 &&
	    strcmp("md4", skey.logname + strlen(skey.logname) + 1) != 0)
		oldmd4 = 1;

	/* Set hash type if asked to */
	if (ht && strcmp(ht, skey_get_algorithm()) != 0)
		skey_set_algorithm(ht);

	alarm(180);
	if (!defaultsetup)
		secure_mode(&n, key, seed, defaultseed, buf, sizeof(buf));
	else
		normal_mode(pp->pw_name, n, key, seed, defaultseed);
	alarm(0);

	(void)time(&now);
	tm = localtime(&now);
	(void)strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	/* If this is an exiting entry, compute the line length and seed pad */
	seedlen = SKEY_MAX_SEED_LEN;
	if (rval == 0) {
		int nlen;

		nlen = strlen(pp->pw_name) + 1 + strlen(skey_get_algorithm()) +
		    1 + 4 + 1 + strlen(seed) + 1 + 16 + 1 + strlen(tbuf) + 1;

		/*
		 * If there was no hash type (md4) add one unless we
		 * are short on space.
		 */ 
		if (oldmd4) {
			if (nlen > skey.len)
				nlen -= 4;
			else
				oldmd4 = 0;
		}

		/* If new entry is longer than the old, comment out the old. */
		if (nlen > skey.len) {
			(void)skeyzero(&skey);
			/* Re-open keys file and seek to the end */
			if (skeylookup(&skey, pp->pw_name) == -1)
				err(1, "cannot reopen database");
			lockeof(&skey, pp->pw_name);
		} else {
			/* Compute how much to space-pad the seed */
			seedlen = strlen(seed) + (skey.len - nlen);
		}
	}

	if ((skey.val = (char *)malloc(16 + 1)) == NULL)
		err(1, "Can't allocate memory");
	btoa8(skey.val, key);

	/* Don't save algorithm type for md4 (maintain record length) */
	/* XXX - should check return values of fprintf + fclose */
	if (oldmd4)
		(void)fprintf(skey.keyfile, "%s %04d %-*s %s %-21s\n",
		    pp->pw_name, n, seedlen, seed, skey.val, tbuf);
	else
		(void)fprintf(skey.keyfile, "%s %s %04d %-*s %s %-21s\n",
		    pp->pw_name, skey_get_algorithm(), n, seedlen, seed,
		    skey.val, tbuf);
	(void)fclose(skey.keyfile);

	(void)printf("\nID %s skey is otp-%s %d %s\n", pp->pw_name,
		     skey_get_algorithm(), n, seed);
	(void)printf("Next login password: %s\n\n",
	    hexmode ? put8(buf, key) : btoe(buf, key));
	exit(0);
}

void
lockeof(mp, user)
	struct skey *mp;
	char *user;
{
	struct flock fl;

	fseek(mp->keyfile, 0, SEEK_END);
dolock:
	fl.l_start = ftell(mp->keyfile);
	fl.l_len = mp->len;
	fl.l_pid = getpid();
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;

	if (fcntl(fileno(mp->keyfile), F_SETLKW, &fl) == -1)
		err(1, "Can't lock database");

	/* Make sure we are still at the end. */
	fseek(mp->keyfile, 0, SEEK_END);
	if (fl.l_start == ftell(mp->keyfile))
		return;		/* still at EOF */

	fclose(mp->keyfile);
	if (skeylookup(mp, user) != 1)
		errx(1, "user %s already added", user);
	goto dolock;
}

void
secure_mode(count, key, seed, defaultseed, buf, bufsiz)
	int *count;
	char *key;
	char *seed;
	char *defaultseed;
	char *buf;
	size_t bufsiz;
{
	int i, n;
	char *p;

	(void)puts("You need the 6 words generated from the \"skey\" command.");
	for (i = 0; ; i++) {
		if (i >= 2)
			exit(1);

		(void)printf("Enter sequence count from 1 to %d: ",
		    SKEY_MAX_SEQ);
		(void)fgets(buf, bufsiz, stdin);
		clearerr(stdin);
		n = atoi(buf);
		if (n > 0 && n < SKEY_MAX_SEQ)
			break;	/* Valid range */
		(void)fprintf(stderr, "ERROR: Count must be between 1 and %d\n",
			     SKEY_MAX_SEQ);
	}

	for (i = 0; ; i++) {
		if (i >= 2)
			exit(1);

		(void)printf("Enter new seed [default %s]: ",
			     defaultseed);
		(void)fgets(seed, SKEY_MAX_SEED_LEN+2, stdin); /* XXX */
		clearerr(stdin);
		rip(seed);
		if (strlen(seed) > SKEY_MAX_SEED_LEN) {
			(void)fprintf(stderr, "ERROR: Seed must be between 1 "
			    "and %d characters in length\n", SKEY_MAX_SEED_LEN);
			continue;
		}
		if (seed[0] == '\0')
			(void)strcpy(seed, defaultseed);
		for (p = seed; *p; p++) {
			if (isspace(*p)) {
				(void)fputs("ERROR: Seed must not contain "
				    "any spaces\n", stderr);
				break;
			} else if (isalpha(*p)) {
				if (isupper(*p))
					*p = tolower(*p);
			} else if (!isdigit(*p)) {
				(void)fputs("ERROR: Seed must be purely "
				    "alphanumeric\n", stderr);
				break;
			}
		}
		if (*p == '\0')
			break;  /* Valid seed */
	}

	for (i = 0; ; i++) {
		if (i >= 2)
			exit(1);

		(void)printf("otp-%s %d %s\nS/Key access password: ",
			     skey_get_algorithm(), n, seed);
		(void)fgets(buf, bufsiz, stdin);
		clearerr(stdin);
		rip(buf);
		backspace(buf);

		if (buf[0] == '?') {
			(void)puts("Enter 6 words from secure S/Key calculation.");
			continue;
		} else if (buf[0] == '\0')
			exit(1);

		if (etob(key, buf) == 1 || atob8(key, buf) == 0)
			break;	/* Valid format */
		(void)fputs("ERROR: Invalid format - try again with the 6 words.\n",
		    stderr);
	}
	*count= n;
}

void
normal_mode(username, n, key, seed, defaultseed)
	char *username;
	int n;
	char *key;
	char *seed;
	char *defaultseed;
{
	int i, nn;
	char passwd[SKEY_MAX_PW_LEN+2], passwd2[SKEY_MAX_PW_LEN+2];

	/* Get user's secret passphrase */
	for (i = 0; ; i++) {
		if (i > 2)
			exit(1);

		if (readpassphrase("Enter secret passphrase: ", passwd,
		    sizeof(passwd), 0) == NULL || passwd[0] == '\0')
			exit(1);

		if (strlen(passwd) < SKEY_MIN_PW_LEN) {
			(void)fprintf(stderr,
			    "ERROR: Your passphrase must be at least %d "
			    "characters long.\n", SKEY_MIN_PW_LEN);
			continue;
		} else if (strcmp(passwd, username) == 0) {
			(void)fputs("ERROR: Your passphrase may not be the "
			    "same as your user name.\n", stderr);
			continue;
		} else if (strspn(passwd, "abcdefghijklmnopqrstuvwxyz") == 
		    strlen(passwd)) {
			(void)fputs("ERROR: Your passphrase must contain more "
			    "than just lower case letters.\nWhitespace, "
			    "numbers, and puctuation are suggested.\n", stderr);
			continue;
		} else if (strlen(passwd) > 63) {
			(void)fprintf(stderr, "WARNING: Your passphrase is "
			    "longer than the recommended maximum length of 63\n");
		}
		/* XXX - should check for passphrase that is really too long */

		if (readpassphrase("Again secret passphrase: ", passwd2,
		    sizeof(passwd2), 0) && strcmp(passwd, passwd2) == 0)
			break;

		(void)fputs("Passphrases do not match.\n", stderr);
	}

	/* Crunch seed and passphrase into starting key */
	(void)strcpy(seed, defaultseed);
	if (keycrunch(key, seed, passwd) != 0)
		err(2, "key crunch failed");

	nn = n;
	while (nn-- != 0)
		f(key);
}

#define TIMEOUT_MSG	"Timed out waiting for input.\n"
void
timedout(signo)
	int signo;
{

	write(STDERR_FILENO, TIMEOUT_MSG, sizeof(TIMEOUT_MSG) - 1);
	_exit(1);
}

void
usage(s)
	char *s;
{
	(void)fprintf(stderr,
		"Usage: %s [-s] [-x] [-z] [-n count] [-md4|-md5|-sha1|-rmd160] [user]\n", s);
	exit(1);
}

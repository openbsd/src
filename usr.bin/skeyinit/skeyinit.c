/*	$OpenBSD: skeyinit.c,v 1.74 2019/01/25 00:19:26 millert Exp $	*/

/* OpenBSD S/Key (skeyinit.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <millert@openbsd.org>
 *
 * S/Key initialization and seed update
 */

#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <readpassphrase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <limits.h>
#include <utmp.h>

#include <skey.h>
#include <bsd_auth.h>

#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN    4
#endif

void	usage(void);
void	secure_mode(int *, char *, char *, size_t, char *, size_t);
void	normal_mode(char *, int, char *, char *);
void	enable_db(int);

int
main(int argc, char **argv)
{
	int     rval, i, l, n, defaultsetup, rmkey, hexmode, enable;
	char	hostname[HOST_NAME_MAX+1];
	char	seed[SKEY_MAX_SEED_LEN + 1];
	char    buf[256], key[SKEY_BINKEY_SIZE], filename[PATH_MAX], *ht;
	char    lastc, *p, *auth_type;
	const char *errstr;
	struct skey skey;
	struct passwd *pp;

	n = rmkey = hexmode = enable = 0;
	defaultsetup = 1;
	ht = auth_type = NULL;

	for (i = 1; i < argc && argv[i][0] == '-' && strcmp(argv[i], "--");) {
		if (argv[i][2] == '\0') {
			/* Single character switch */
			switch (argv[i][1]) {
			case 'a':
				if (argv[++i] == NULL || argv[i][0] == '\0')
					usage();
				auth_type = argv[i];
				break;
			case 's':
				defaultsetup = 0;
				if (auth_type == NULL)
					auth_type = "skey";
				break;
			case 'x':
				hexmode = 1;
				break;
			case 'r':
				rmkey = 1;
				break;
			case 'n':
				if (argv[++i] == NULL || argv[i][0] == '\0')
					usage();
				n = strtonum(argv[i], 1, SKEY_MAX_SEQ - 1, &errstr);
				if (errstr)
					errx(1, "count must be > 0 and < %d",
					     SKEY_MAX_SEQ);
				break;
			case 'D':
				enable = -1;
				break;
			case 'E':
				enable = 1;
				break;
			default:
				usage();
			}
		} else {
			/* Multi character switches are hash types */
			if ((ht = skey_set_algorithm(&argv[i][1])) == NULL) {
				warnx("Unknown hash algorithm %s", &argv[i][1]);
				usage();
			}
		}
		i++;
	}
	argv += i;
	argc -= i;

	if (argc > 1 || (enable && argc))
		usage();

	/* Handle -D and -E */
	if (enable) {
		enable_db(enable);
		exit(0);
	}

	if (getuid() != 0) {
		if (pledge("stdio rpath wpath cpath fattr flock tty proc exec "
		    "getpw", NULL) == -1)
			err(1, "pledge");

		if ((pp = getpwuid(getuid())) == NULL)
			err(1, "no user with uid %u", getuid());

		if (argc == 1) {
			char me[UT_NAMESIZE + 1]; 

			(void)strlcpy(me, pp->pw_name, sizeof me);
			if ((pp = getpwnam(argv[0])) == NULL)
				errx(1, "User unknown: %s", argv[0]);
			if (strcmp(pp->pw_name, me) != 0)
				errx(1, "Permission denied.");
		}
	} else {
		if (pledge("stdio rpath wpath cpath fattr flock tty getpw id",
		    NULL) == -1)
			err(1, "pledge");

		if (argc == 1) {
			if ((pp = getpwnam(argv[0])) == NULL) {
				static struct passwd _pp;

				_pp.pw_name = argv[0];
				pp = &_pp;
				warnx("Warning, user unknown: %s", argv[0]);
			} else {
				/* So the file ends up owned by the proper ID */
				if (setresuid(-1, pp->pw_uid, -1) != 0)
					errx(1, "unable to change uid to %u",
					    pp->pw_uid);
			}
		} else if ((pp = getpwuid(0)) == NULL)
			err(1, "no user with uid 0");

		if (pledge("stdio rpath wpath cpath fattr flock tty", NULL)
		    == -1)
			err(1, "pledge");
	}

	switch (skey_haskey(pp->pw_name)) {
	case -1:
		if (errno == ENOENT || errno == EPERM)
			errx(1, "S/Key disabled");
		else
			err(1, "cannot open database");
		break;
	case 0:
		/* existing user */
		break;
	case 1:
		if (!defaultsetup && strcmp(auth_type, "skey") == 0) {
			fprintf(stderr,
"You must authenticate yourself before using S/Key for the first time.  In\n"
"secure mode this is normally done via an existing S/Key key.  However, since\n"
"you do not have an entry in the S/Key database you will have to specify an\n"
"alternate authentication type via the `-a' flag, e.g.\n"
"    \"skeyinit -s -a passwd\"\n\n"
"Note that entering a plaintext password over a non-secure link defeats the\n"
"purpose of using S/Key in the fist place.\n");
			exit(1);
		}
		break;
	}

	if (getuid() != 0) {
		if ((pp = pw_dup(pp)) == NULL)
			err(1, NULL);
		if (!auth_userokay(pp->pw_name, auth_type, NULL, NULL))
			errx(1, "Password incorrect");
	}

	if (pledge("stdio rpath wpath cpath fattr flock tty", NULL) == -1)
		err(1, "pledge");

	/* Build up a default seed based on the hostname and some randomness */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	for (i = 0, p = seed; hostname[i] && i < SKEY_NAMELEN; i++) {
		if (isalnum((unsigned char)hostname[i]))
			*p++ = tolower((unsigned char)hostname[i]);
	}
	for (i = 0; i < 5; i++)
		*p++ = arc4random_uniform(10) + '0';
	*p = '\0';

	/*
	 * Lookup and lock the record we are about to modify.
	 * If this is a new entry this will prevent other users
	 * from appending new entries (and clobbering ours).
	 */
	rval = skeylookup(&skey, pp->pw_name);
	switch (rval) {
		case -1:
			err(1, "cannot open database");
			break;
		case 0:
			/* remove user if asked to do so */
			if (rmkey) {
				if (snprintf(filename, sizeof(filename),
				    "%s/%s", _PATH_SKEYDIR, pp->pw_name)
				    >= sizeof(filename))
					errc(1, ENAMETOOLONG,
					    "Cannot remove S/Key entry");
				if (unlink(filename) != 0)
					err(1, "Cannot remove S/Key entry");
				printf("S/Key entry for %s removed.\n",
				    pp->pw_name);
				exit(0);
			}

			(void)printf("[Updating %s with %s]\n", pp->pw_name,
			    ht ? ht : skey_get_algorithm());
			(void)printf("Old seed: [%s] %s\n",
				     skey_get_algorithm(), skey.seed);

			/*
			 * Sanity check old seed.
			 */
			l = strlen(skey.seed);
			for (p = skey.seed; *p; p++) {
				if (isalpha((unsigned char)*p)) {
					if (isupper((unsigned char)*p))
						*p = tolower((unsigned char)*p);
				} else if (!isdigit((unsigned char)*p)) {
					memmove(p, p + 1, l - (p - skey.seed));
					l--;
				}
			}

			/* If the seed ends in 0-8 just add one.  */
			if (l > 0) {
				lastc = skey.seed[l - 1];
				if (isdigit((unsigned char)lastc) &&
				    lastc != '9') {
					(void)strlcpy(seed, skey.seed,
					    sizeof seed);
					seed[l - 1] = lastc + 1;
				}
				if (isdigit((unsigned char)lastc) &&
				    lastc == '9' && l < 16) {
					(void)strlcpy(seed, skey.seed,
					    sizeof seed);
					seed[l - 1] = '0';
					seed[l] = '0';
					seed[l + 1] = '\0';
				}
			}
			break;
		case 1:
			if (rmkey)
				errx(1, "You have no entry to remove.");
			(void)printf("[Adding %s with %s]\n", pp->pw_name,
			    ht ? ht : skey_get_algorithm());
			if (snprintf(filename, sizeof(filename), "%s/%s",
			    _PATH_SKEYDIR, pp->pw_name) >= sizeof(filename))
				errc(1, ENAMETOOLONG,
				    "Cannot create S/Key entry");
			if ((l = open(filename,
			    O_RDWR | O_NONBLOCK | O_CREAT | O_TRUNC |O_NOFOLLOW,
			    S_IRUSR | S_IWUSR)) == -1 ||
			    flock(l, LOCK_EX) != 0 ||
			    (skey.keyfile = fdopen(l, "r+")) == NULL)
				err(1, "Cannot create S/Key entry");
			break;
	}
	if (fchown(fileno(skey.keyfile), pp->pw_uid, -1) != 0 ||
	    fchmod(fileno(skey.keyfile), S_IRUSR | S_IWUSR) != 0)
		err(1, "can't set owner/mode for %s", pp->pw_name);
	if (defaultsetup && n == 0)
		n = 100;

	/* Set hash type if asked to */
	if (ht && strcmp(ht, skey_get_algorithm()) != 0)
		skey_set_algorithm(ht);

	alarm(180);
	if (!defaultsetup)
		secure_mode(&n, key, seed, sizeof seed, buf, sizeof(buf));
	else
		normal_mode(pp->pw_name, n, key, seed);
	alarm(0);

	/* XXX - why use malloc here? */
	if ((skey.val = malloc(16 + 1)) == NULL)
		err(1, "Can't allocate memory");
	btoa8(skey.val, key);

	(void)fseek(skey.keyfile, 0L, SEEK_SET);
	(void)fprintf(skey.keyfile, "%s\n%s\n%04d\n%s\n%s\n",
	    pp->pw_name, skey_get_algorithm(), n, seed, skey.val);
	(void)fclose(skey.keyfile);

	(void)printf("\nID %s skey is otp-%s %d %s\n", pp->pw_name,
	    skey_get_algorithm(), n, seed);
	(void)printf("Next login password: %s\n\n",
	    hexmode ? put8(buf, key) : btoe(buf, key));
	exit(0);
}

void
secure_mode(int *count, char *key, char *seed, size_t seedlen,
    char *buf, size_t bufsiz)
{
	char *p, newseed[SKEY_MAX_SEED_LEN + 2];
	const char *errstr;
	int i, n = *count;

	(void)puts("You need the 6 words generated from the \"skey\" command.");
	if (n == 0) {
		for (i = 0; ; i++) {
			if (i >= 2)
				exit(1);

			(void)printf("Enter sequence count from 1 to %d: ",
			    SKEY_MAX_SEQ);
			(void)fgets(buf, bufsiz, stdin);
			clearerr(stdin);
			rip(buf);
			n = strtonum(buf, 1, SKEY_MAX_SEQ-1, &errstr);
			if (!errstr)
				break;	/* Valid range */
			fprintf(stderr,
			    "ERROR: Count must be between 1 and %d\n",
			    SKEY_MAX_SEQ - 1);
		}
		*count= n;
	}

	for (i = 0; ; i++) {
		if (i >= 2)
			exit(1);

		(void)printf("Enter new seed [default %s]: ", seed);
		(void)fgets(newseed, sizeof(newseed), stdin); /* XXX */
		clearerr(stdin);
		rip(newseed);
		if (strlen(newseed) > SKEY_MAX_SEED_LEN) {
			(void)fprintf(stderr, "ERROR: Seed must be between 1 "
			    "and %d characters in length\n", SKEY_MAX_SEED_LEN);
			continue;
		}
		for (p = newseed; *p; p++) {
			if (isspace((unsigned char)*p)) {
				(void)fputs("ERROR: Seed must not contain "
				    "any spaces\n", stderr);
				break;
			} else if (isalpha((unsigned char)*p)) {
				if (isupper((unsigned char)*p))
					*p = tolower((unsigned char)*p);
			} else if (!isdigit((unsigned char)*p)) {
				(void)fputs("ERROR: Seed must be purely "
				    "alphanumeric\n", stderr);
				break;
			}
		}
		if (*p == '\0')
			break;  /* Valid seed */
	}
	if (newseed[0] != '\0')
		(void)strlcpy(seed, newseed, seedlen);

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
}

void
normal_mode(char *username, int n, char *key, char *seed)
{
	int i, nn;
	char passwd[SKEY_MAX_PW_LEN+2], key2[SKEY_BINKEY_SIZE];

	/* Get user's secret passphrase */
	for (i = 0; ; i++) {
		if (i > 2)
			errx(1, "S/Key entry not updated");

		if (readpassphrase("Enter new secret passphrase: ", passwd,
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
			    "numbers, and punctuation are suggested.\n",
			    stderr);
			continue;
		} else if (strlen(passwd) > 63) {
			(void)fprintf(stderr, "WARNING: Your passphrase is "
			    "longer than the recommended maximum length of 63\n");
		}
		/* XXX - should check for passphrase that is really too long */

		/* Crunch seed and passphrase into starting key */
		nn = keycrunch(key, seed, passwd);
		explicit_bzero(passwd, sizeof(passwd));
		if (nn != 0)
			err(2, "key crunch failed");

		if (readpassphrase("Again secret passphrase: ", passwd,
		    sizeof(passwd), 0) == NULL || passwd[0] == '\0')
			exit(1);

		/* Crunch seed and passphrase into starting key */
		nn = keycrunch(key2, seed, passwd);
		explicit_bzero(passwd, sizeof(passwd));
		if (nn != 0)
			err(2, "key crunch failed");

		if (memcmp(key, key2, sizeof(key2)) == 0)
			break;

		(void)fputs("Passphrases do not match.\n", stderr);
	}

	nn = n;
	while (nn-- != 0)
		f(key);
}

void
enable_db(int op)
{
	if (op == 1) {
		/* enable */
		if (mkdir(_PATH_SKEYDIR, 01730) != 0 && errno != EEXIST)
			err(1, "can't mkdir %s", _PATH_SKEYDIR);
		if (chown(_PATH_SKEYDIR, geteuid(), getegid()) != 0)
			err(1, "can't chown %s", _PATH_SKEYDIR);
		if (chmod(_PATH_SKEYDIR, 01730) != 0)
			err(1, "can't chmod %s", _PATH_SKEYDIR);
	} else {
		/* disable */
		if (chmod(_PATH_SKEYDIR, 0) != 0 && errno != ENOENT)
			err(1, "can't chmod %s", _PATH_SKEYDIR);
	}
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-DErsx] [-a auth-type] [-n count]"
	    "\n\t[-md5 | -rmd160 | -sha1] [user]\n", __progname);
	exit(1);
}

/*	$OpenBSD: skeyinit.c,v 1.49 2004/09/14 22:58:57 deraadt Exp $	*/

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
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
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
#include <bsd_auth.h>

#ifndef SKEY_NAMELEN
#define SKEY_NAMELEN    4
#endif

void	usage(void);
void	secure_mode(int *, char *, char *, size_t, char *, size_t);
void	normal_mode(char *, int, char *, char *);
void	convert_db(void);
void	enable_db(int);

int
main(int argc, char **argv)
{
	int     rval, i, l, n, defaultsetup, rmkey, hexmode, enable, convert;
	char	hostname[MAXHOSTNAMELEN];
	char	seed[SKEY_MAX_SEED_LEN + 1];
	char    buf[256], key[SKEY_BINKEY_SIZE], filename[PATH_MAX], *ht;
	char    lastc, me[UT_NAMESIZE + 1], *p, *auth_type;
	u_int32_t noise;
	struct skey skey;
	struct passwd *pp;

	n = rmkey = hexmode = enable = convert = 0;
	defaultsetup = 1;
	ht = auth_type = NULL;

	/* Build up a default seed based on the hostname and some noise */
	if (gethostname(hostname, sizeof(hostname)) < 0)
		err(1, "gethostname");
	for (i = 0, p = seed; hostname[i] && i < SKEY_NAMELEN; i++) {
		if (isalpha(hostname[i])) {
			if (isupper(hostname[i]))
				hostname[i] = tolower(hostname[i]);
			*p++ = hostname[i];
		} else if (isdigit(hostname[i]))
			*p++ = hostname[i];
	}
	noise = arc4random();
	for (i = 0; i < 5; i++) {
		*p++ = (noise % 10) + '0';
		noise /= 10;
	}
	*p = '\0';

	if ((pp = getpwuid(getuid())) == NULL)
		err(1, "no user with uid %u", getuid());
	(void)strlcpy(me, pp->pw_name, sizeof me);

	if ((pp = getpwnam(me)) == NULL)
		err(1, "Who are you?");

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
				if ((n = atoi(argv[i])) < 1 || n >= SKEY_MAX_SEQ)
					errx(1, "count must be > 0 and < %d",
					     SKEY_MAX_SEQ);
				break;
			case 'C':
				convert = 1;
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

	if (argc > 1 || (enable && convert) || (enable && argc) ||
	    (convert && argc))
		usage();

	/* Handle -C, -D, and -E */
	if (convert || enable) {
		if (convert)
			convert_db();
		else
			enable_db(enable);
		exit(0);
	}

	/* Check for optional user string. */
	if (argc == 1) {
		if ((pp = getpwnam(argv[0])) == NULL) {
			if (getuid() == 0) {
				static struct passwd _pp;

				_pp.pw_name = argv[0];
				pp = &_pp;
				warnx("Warning, user unknown: %s", argv[0]);
			} else {
				errx(1, "User unknown: %s", argv[0]);
			}
		} else if (strcmp(pp->pw_name, me) != 0 && getuid() != 0) {
			/* Only root can change other's S/Keys. */
			errx(1, "Permission denied.");
		}
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
"    \"skeyinit -s -a krb5\" or \"skeyinit -s -a passwd\"\n\n"
"Note that entering a plaintext password over a non-secure link defeats the\n"
"purpose of using S/Key in the fist place.\n");
			exit(1);
		}
		break;
	}

	if (defaultsetup)
		fputs("Reminder - Only use this method if you are directly "
		    "connected\n           or have an encrypted channel.  If "
		    "you are using telnet,\n           hit return now and use "
		    "skeyinit -s.\n", stderr);

	if (getuid() != 0) {
		if ((pp = pw_dup(pp)) == NULL)
			err(1, NULL);
		if (!auth_userokay(pp->pw_name, auth_type, NULL, NULL))
			errx(1, "Password incorrect");
	}

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
				    >= sizeof(filename)) {
					errno = ENAMETOOLONG;
					err(1, "Cannot remove S/Key entry");
				}
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
					(void)strlcpy(seed, skey.seed,
					    sizeof seed);
					seed[l - 1] = lastc + 1;
				}
				if (isdigit(lastc) && lastc == '9' && l < 16) {
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
			    _PATH_SKEYDIR, pp->pw_name) >= sizeof(filename)) {
				errno = ENAMETOOLONG;
				err(1, "Cannot create S/Key entry");
			}
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
	if (n == 0)
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
	if ((skey.val = (char *)malloc(16 + 1)) == NULL)
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
	int i, n;

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
	*count= n;
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
		memset(passwd, 0, sizeof(passwd));
		if (nn != 0)
			err(2, "key crunch failed");

		if (readpassphrase("Again secret passphrase: ", passwd,
		    sizeof(passwd), 0) == NULL || passwd[0] == '\0')
			exit(1);

		/* Crunch seed and passphrase into starting key */
		nn = keycrunch(key2, seed, passwd);
		memset(passwd, 0, sizeof(passwd));
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

#define _PATH_SKEYKEYS	"/etc/skeykeys"
void
convert_db(void)
{
	struct passwd *pp;
	uid_t uid;
	FILE *keyfile;
	FILE *newfile;
	char buf[256], *logname, *hashtype, *seed, *val, *cp;
	char filename[PATH_MAX];
	int fd, n;

	if ((keyfile = fopen(_PATH_SKEYKEYS, "r")) == NULL)
		err(1, "can't open %s", _PATH_SKEYKEYS);
	if (flock(fileno(keyfile), LOCK_EX) != 0)
		err(1, "can't lock %s", _PATH_SKEYKEYS);
	enable_db(1);

	/*
	 * Loop over each entry in _PATH_SKEYKEYS, creating a file
	 * in _PATH_SKEYDIR for each one.
	 */
	while (fgets(buf, sizeof(buf), keyfile) != NULL) {
		if (buf[0] == '#')
			continue;
		if ((logname = strtok(buf, " \t")) == NULL)
			continue;
		if ((cp = strtok(NULL, " \t")) == NULL)
			continue;
		if (isalpha(*cp)) {
			hashtype = cp;
			if ((cp = strtok(NULL, " \t")) == NULL)
				continue;
		} else
			hashtype = "md4";
		n = atoi(cp);
		if ((seed = strtok(NULL, " \t")) == NULL)
			continue;
		if ((val = strtok(NULL, " \t")) == NULL)
			continue;

		if ((pp = getpwnam(logname)) != NULL)
			uid = pp->pw_uid;
		else
			uid = 0;

		/* Now write the new-style record. */
		if (snprintf(filename, sizeof(filename), "%s/%s", _PATH_SKEYDIR,
		    logname) >= sizeof(filename)) {
			errno = ENAMETOOLONG;
			warn("%s", logname);
			continue;
		}
		fd = open(filename, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if (fd == -1 || flock(fd, LOCK_EX) != 0 ||
		    (newfile = fdopen(fd, "r+")) == NULL) {
			warn("%s", logname);
			continue;
		}
		(void)fprintf(newfile, "%s\n%s\n%04d\n%s\n%s\n", logname,
			    hashtype, n, seed, val);
		(void)fchown(fileno(newfile), uid, -1);
		(void)fclose(newfile);
	}
	printf("%s has been populated.  NOTE: %s has *not* been removed.\n"
	    "It should be removed once you have verified that the new keys "
	    "work.\n", _PATH_SKEYDIR, _PATH_SKEYKEYS);
}

void
usage(void)
{
	extern char *__progname;

	(void)fprintf(stderr, "usage: %s [-r] [-s] [-x] [-C] [-D] [-E] "
	    "[-a auth_type] [-n count]\n                "
	    "[-md4|-md5|-sha1|-rmd160] [user]\n", __progname);
	exit(1);
}

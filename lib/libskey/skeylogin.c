/* OpenBSD S/Key (skeylogin.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *          Todd C. Miller <Todd.Miller@courtesan.com>
 *	    Angelos D. Keromytis <adk@adk.gr>
 *
 * S/Key verification check, lookups, and authentication.
 *
 * $OpenBSD: skeylogin.c,v 1.48 2002/11/16 22:54:46 millert Exp $
 */

#include <sys/param.h>
#ifdef	QUOTA
#include <sys/quota.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sha1.h>

#include "skey.h"

static void skey_fakeprompt(char *, char *);
static char *tgetline(int, char *, size_t, int);

/*
 * Return an skey challenge string for user 'name'. If successful,
 * fill in the caller's skey structure and return (0). If unsuccessful
 * (e.g., if name is unknown) return (-1).
 *
 * The file read/write pointer is left at the start of the
 * record.
 */
int
skeychallenge(mp, name, ss)
	struct skey *mp;
	char *name;
	char *ss;
{
	int rval;

	rval = skeylookup(mp, name);
	switch (rval) {
	case 0:		/* Lookup succeeded, return challenge */
		(void)snprintf(ss, SKEY_MAX_CHALLENGE,
		    "otp-%.*s %d %.*s", SKEY_MAX_HASHNAME_LEN,
			      skey_get_algorithm(), mp->n - 1,
			      SKEY_MAX_SEED_LEN, mp->seed);
		return (0);

	case 1:		/* User not found */
		if (mp->keyfile) {
			(void)fclose(mp->keyfile);
			mp->keyfile = NULL;
		}
		/* FALLTHROUGH */

	default:	/* File error */
		skey_fakeprompt(name, ss);
		return (-1);
	}
}

/*
 * Find an entry in the One-time Password database and lock it.
 *
 * Return codes:
 * -1: error in opening database or unable to lock entry
 *  0: entry found, file R/W pointer positioned at beginning of record
 *  1: entry not found
 */
int
skeylookup(mp, name)
	struct skey *mp;
	char *name;
{
	struct stat statbuf;
	size_t nread;
	char *cp, filename[PATH_MAX], *last;
	FILE *keyfile;
	int fd;

	memset(mp, 0, sizeof(*mp));

	/* Check to see that /etc/skey has not been disabled. */
	if (stat(_PATH_SKEYDIR, &statbuf) != 0)
		return (-1);
	if ((statbuf.st_mode & ALLPERMS) == 0) {
		errno = EPERM;
		return (-1);
	}

	/* Open the user's databse entry, creating it as needed. */
	/* XXX - really want "/etc/skey/L/USER" where L is 1st char of USER */
	if (snprintf(filename, sizeof(filename), "%s/%s", _PATH_SKEYDIR,
	    name) >= sizeof(filename)) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	if ((fd = open(filename, O_RDWR | O_NOFOLLOW | O_NONBLOCK,
	    S_IRUSR | S_IWUSR)) == -1) {
		if (errno == ENOENT)
			goto not_found;
		return (-1);
	}

	/* Lock and stat the user's skey file. */
	if (flock(fd, LOCK_EX) != 0 || fstat(fd, &statbuf) != 0) {
		close(fd);
		return (-1);
	}
	if (statbuf.st_size == 0)
		goto not_found;

	/* Sanity checks. */
	if ((statbuf.st_mode & ALLPERMS) != (S_IRUSR | S_IWUSR) ||
	    !S_ISREG(statbuf.st_mode) || statbuf.st_nlink != 1 ||
	    (keyfile = fdopen(fd, "r+")) == NULL) {
		close(fd);
		return (-1);
	}

	/* At this point, we are committed. */
	mp->keyfile = keyfile;

	if ((nread = fread(mp->buf, 1, sizeof(mp->buf), keyfile)) == 0 ||
	    !isspace(mp->buf[nread - 1]))
		goto bad_keyfile;
	mp->buf[nread - 1] = '\0';

	if ((mp->logname = strtok_r(mp->buf, " \t\n\r", &last)) == NULL ||
	    strcmp(mp->logname, name) != 0)
		goto bad_keyfile;
	if ((cp = strtok_r(NULL, " \t\n\r", &last)) == NULL)
		goto bad_keyfile;
	if (skey_set_algorithm(cp) == NULL)
		goto bad_keyfile;
	if ((cp = strtok_r(NULL, " \t\n\r", &last)) == NULL)
		goto bad_keyfile;
	mp->n = atoi(cp);	/* XXX - use strtol() */
	if ((mp->seed = strtok_r(NULL, " \t\n\r", &last)) == NULL)
		goto bad_keyfile;
	if ((mp->val = strtok_r(NULL, " \t\n\r", &last)) == NULL)
		goto bad_keyfile;

	(void)fseek(keyfile, 0L, SEEK_SET);
	return (0);

    bad_keyfile:
	fclose(keyfile);
	return (-1);

    not_found:
	/* No existing entry, fill in what we can and return */
	memset(mp, 0, sizeof(*mp));
	strlcpy(mp->buf, name, sizeof(mp->buf));
	mp->logname = mp->buf;
	if (fd != -1)
		close(fd);
	return (1);
}

/*
 * Get the next entry in the One-time Password database.
 *
 * Return codes:
 * -1: error in opening database
 *  0: next entry found and stored in mp
 *  1: no more entries, keydir is closed.
 */
int
skeygetnext(mp)
	struct skey *mp;
{
	struct dirent entry, *dp;
	int rval;

	if (mp->keyfile != NULL) {
		fclose(mp->keyfile);
		mp->keyfile = NULL;
	}

	/* Open _PATH_SKEYDIR if it exists, else return an error */
	if (mp->keydir == NULL && (mp->keydir = opendir(_PATH_SKEYDIR)) == NULL)
		return (-1);

	rval = 1;
	while ((readdir_r(mp->keydir, &entry, &dp)) == 0 && dp == &entry) {
		/* Skip dot files and zero-length files. */
		if (entry.d_name[0] != '.' &&
		    (rval = skeylookup(mp, entry.d_name) != 1))
			break;
	};

	if (dp == NULL) {
		closedir(mp->keydir);
		mp->keydir = NULL;
	}

	return (rval);
}

/*
 * Verify response to a S/Key challenge.
 *
 * Return codes:
 * -1: Error of some sort; database unchanged
 *  0:  Verify successful, database updated
 *  1:  Verify failed, database unchanged
 *
 * The database file is always closed by this call.
 */
int
skeyverify(mp, response)
	struct skey *mp;
	char *response;
{
	char key[SKEY_BINKEY_SIZE];
	char fkey[SKEY_BINKEY_SIZE];
	char filekey[SKEY_BINKEY_SIZE];
	size_t nread;
	char *cp, *last;

	if (response == NULL)
		goto verify_failure;

	/*
	 * The record should already be locked but lock it again
	 * just to be safe.  We don't wait for the lock to become
	 * available since we should already have it...
	 */
	if (flock(fileno(mp->keyfile), LOCK_EX | LOCK_NB) != 0)
		goto verify_failure;

	/* Convert response to binary */
	rip(response);
	if (etob(key, response) != 1 && atob8(key, response) != 0)
		goto verify_failure; /* Neither english words nor ascii hex */

	/* Compute fkey = f(key) */
	(void)memcpy(fkey, key, sizeof(key));
	f(fkey);

	/*
	 * Reread the file record NOW in case it has been modified.
	 * The only field we really need to worry about is mp->val.
	 */
	(void)fseek(mp->keyfile, 0L, SEEK_SET);
	if ((nread = fread(mp->buf, 1, sizeof(mp->buf), mp->keyfile)) == 0 ||
	    !isspace(mp->buf[nread - 1]))
		goto verify_failure;
	if ((mp->logname = strtok_r(mp->buf, " \t\r\n", &last)) == NULL)
		goto verify_failure;
	if ((cp = strtok_r(NULL, " \t\r\n", &last)) == NULL)
		goto verify_failure;
	if ((cp = strtok_r(NULL, " \t\r\n", &last)) == NULL)
		goto verify_failure;
	if ((mp->seed = strtok_r(NULL, " \t\r\n", &last)) == NULL)
		goto verify_failure;
	if ((mp->val = strtok_r(NULL, " \t\r\n", &last)) == NULL)
		goto verify_failure;

	/* Convert file value to hex and compare. */
	atob8(filekey, mp->val);
	if (memcmp(filekey, fkey, SKEY_BINKEY_SIZE) != 0)
		goto verify_failure;	/* Wrong response */

	/*
	 * Update key in database.
	 * XXX - check return values of things that write to disk.
	 */
	btoa8(mp->val,key);
	mp->n--;
	(void)fseek(mp->keyfile, 0L, SEEK_SET);
	(void)fprintf(mp->keyfile, "%s\n%s\n%d\n%s\n%s\n", mp->logname,
	    skey_get_algorithm(), mp->n, mp->seed, mp->val);
	(void)fflush(mp->keyfile);
	(void)ftruncate(fileno(mp->keyfile), ftello(mp->keyfile));
	(void)fclose(mp->keyfile);
	mp->keyfile = NULL;
	return (0);

    verify_failure:
	(void)fclose(mp->keyfile);
	mp->keyfile = NULL;
	return (-1);
}

/*
 * skey_haskey()
 *
 * Returns: 1 user doesn't exist, -1 file error, 0 user exists.
 *
 */
int
skey_haskey(username)
	char *username;
{
	struct skey skey;
	int i;

	i = skeylookup(&skey, username);
	if (skey.keyfile != NULL) {
		fclose(skey.keyfile);
		skey.keyfile = NULL;
	}
	return (i);
}

/*
 * skey_keyinfo()
 *
 * Returns the current sequence number and
 * seed for the passed user.
 *
 */
char *
skey_keyinfo(username)
	char *username;
{
	int i;
	static char str[SKEY_MAX_CHALLENGE];
	struct skey skey;

	i = skeychallenge(&skey, username, str);
	if (i == -1)
		return (0);

	if (skey.keyfile != NULL) {
		fclose(skey.keyfile);
		skey.keyfile = NULL;
	}
	return (str);
}

/*
 * skey_passcheck()
 *
 * Check to see if answer is the correct one to the current
 * challenge.
 *
 * Returns: 0 success, -1 failure
 *
 */
int
skey_passcheck(username, passwd)
	char *username;
	char *passwd;
{
	int i;
	struct skey skey;

	i = skeylookup(&skey, username);
	if (i == -1 || i == 1)
		return (-1);

	if (skeyverify(&skey, passwd) == 0)
		return (skey.n);

	return (-1);
}

#define ROUND(x)   (((x)[0] << 24) + (((x)[1]) << 16) + (((x)[2]) << 8) + \
		    ((x)[3]))

/*
 * hash_collapse()
 */
static u_int32_t
hash_collapse(s)
	u_char *s;
{
	int len, target;
	u_int32_t i;

	if ((strlen(s) % sizeof(u_int32_t)) == 0)
  		target = strlen(s);    /* Multiple of 4 */
	else
		target = strlen(s) - (strlen(s) % sizeof(u_int32_t));

	for (i = 0, len = 0; len < target; len += 4)
		i ^= ROUND(s + len);

	return i;
}

/*
 * skey_fakeprompt()
 *
 * Generate a fake prompt for the specified user.
 *
 */
static void
skey_fakeprompt(username, skeyprompt)
	char *username;
	char *skeyprompt;
{
	int i;
	u_int ptr;
	u_char hseed[SKEY_MAX_SEED_LEN], flg = 1, *up;
	char *secret, pbuf[SKEY_MAX_PW_LEN+1];
	char *p, *u;
	size_t secretlen;
	SHA1_CTX ctx;

	/*
	 * Base first 4 chars of seed on hostname.
	 * Add some filler for short hostnames if necessary.
	 */
	if (gethostname(pbuf, sizeof(pbuf)) == -1)
		*(p = pbuf) = '.';
	else
		for (p = pbuf; *p && isalnum(*p); p++)
			if (isalpha(*p) && isupper(*p))
				*p = tolower(*p);
	if (*p && pbuf - p < 4)
		(void)strncpy(p, "asjd", 4 - (pbuf - p));
	pbuf[4] = '\0';

	/* Hash the username if possible */
	if ((up = SHA1Data(username, strlen(username), NULL)) != NULL) {
		struct stat sb;
		time_t t;
		int fd;

		/* Collapse the hash */
		ptr = hash_collapse(up);
		memset(up, 0, strlen(up));

		/* See if the random file's there, else use ctime */
		if ((fd = open(_SKEY_RAND_FILE_PATH_, O_RDONLY)) != -1
		    && fstat(fd, &sb) == 0 &&
		    sb.st_size > (off_t)SKEY_MAX_SEED_LEN &&
		    lseek(fd, ptr % (sb.st_size - SKEY_MAX_SEED_LEN),
		    SEEK_SET) != -1 && read(fd, hseed,
		    SKEY_MAX_SEED_LEN) == SKEY_MAX_SEED_LEN) {
			close(fd);
			fd = -1;
			secret = hseed;
			secretlen = SKEY_MAX_SEED_LEN;
			flg = 0;
		} else if (!stat(_PATH_MEM, &sb) || !stat("/", &sb)) {
			t = sb.st_ctime;
			secret = ctime(&t);
			secretlen = strlen(secret);
			flg = 0;
		}
		if (fd != -1)
			close(fd);
	}

	/* Put that in your pipe and smoke it */
	if (flg == 0) {
		/* Hash secret value with username */
		SHA1Init(&ctx);
		SHA1Update(&ctx, secret, secretlen);
		SHA1Update(&ctx, username, strlen(username));
		SHA1End(&ctx, up);

		/* Zero out */
		memset(secret, 0, secretlen);

		/* Now hash the hash */
		SHA1Init(&ctx);
		SHA1Update(&ctx, up, strlen(up));
		SHA1End(&ctx, up);

		ptr = hash_collapse(up + 4);

		for (i = 4; i < 9; i++) {
			pbuf[i] = (ptr % 10) + '0';
			ptr /= 10;
		}
		pbuf[i] = '\0';

		/* Sequence number */
		ptr = ((up[2] + up[3]) % 99) + 1;

		memset(up, 0, 20); /* SHA1 specific */
		free(up);

		(void)snprintf(skeyprompt, SKEY_MAX_CHALLENGE,
		    "otp-%.*s %d %.*s", SKEY_MAX_HASHNAME_LEN,
		    skey_get_algorithm(), ptr, SKEY_MAX_SEED_LEN, pbuf);
	} else {
		/* Base last 8 chars of seed on username */
		u = username;
		i = 8;
		p = &pbuf[4];
		do {
			if (*u == 0) {
				/* Pad remainder with zeros */
				while (--i >= 0)
					*p++ = '0';
				break;
			}

			*p++ = (*u++ % 10) + '0';
		} while (--i != 0);
		pbuf[12] = '\0';

		(void)snprintf(skeyprompt, SKEY_MAX_CHALLENGE,
		    "otp-%.*s %d %.*s", SKEY_MAX_HASHNAME_LEN,
		    skey_get_algorithm(), 99, SKEY_MAX_SEED_LEN, pbuf);
	}
}

/*
 * skey_authenticate()
 *
 * Used when calling program will allow input of the user's
 * response to the challenge.
 *
 * Returns: 0 success, -1 failure
 *
 */
int
skey_authenticate(username)
	char *username;
{
	int i;
	char pbuf[SKEY_MAX_PW_LEN+1], skeyprompt[SKEY_MAX_CHALLENGE+1];
	struct skey skey;

	/* Get the S/Key challenge (may be fake) */
	i = skeychallenge(&skey, username, skeyprompt);
	(void)fprintf(stderr, "%s\nResponse: ", skeyprompt);
	(void)fflush(stderr);

	/* Time out on user input after 2 minutes */
	tgetline(fileno(stdin), pbuf, sizeof(pbuf), 120);
	sevenbit(pbuf);
	(void)rewind(stdin);

	/* Is it a valid response? */
	if (i == 0 && skeyverify(&skey, pbuf) == 0) {
		if (skey.n < 5) {
			(void)fprintf(stderr,
			    "\nWarning! Key initialization needed soon.  (%d logins left)\n",
			    skey.n);
		}
		return (0);
	}
	return (-1);
}

/*
 * Unlock current entry in the One-time Password database.
 *
 * Return codes:
 * -1: unable to lock the record
 *  0: record was successfully unlocked
 */
int
skey_unlock(mp)
	struct skey *mp;
{
	if (mp->logname == NULL || mp->keyfile == NULL)
		return (-1);

	return (flock(fileno(mp->keyfile), LOCK_UN));
}

/*
 * Get a line of input (optionally timing out) and place it in buf.
 */
static char *
tgetline(fd, buf, bufsiz, timeout)
	int fd;
	char *buf;
	size_t bufsiz;
	int timeout;
{
	size_t left;
	int n;
	fd_set *readfds = NULL;
	struct timeval tv;
	char c;
	char *cp;

	if (bufsiz == 0)
		return (NULL);			/* sanity */

	cp = buf;
	left = bufsiz;

	/*
	 * Timeout of <= 0 means no timeout.
	 */
	if (timeout > 0) {
		/* Setup for select(2) */
		n = howmany(fd + 1, NFDBITS) * sizeof(fd_mask);
		if ((readfds = (fd_set *) malloc(n)) == NULL)
			return (NULL);
		(void) memset(readfds, 0, n);

		/* Set timeout for select */
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		while (--left) {
			FD_SET(fd, readfds);

			/* Make sure there is something to read (or timeout) */
			while ((n = select(fd + 1, readfds, 0, 0, &tv)) == -1 &&
			    (errno == EINTR || errno == EAGAIN))
				;
			if (n == 0) {
				free(readfds);
				return (NULL);		/* timeout */
			}

			/* Read a character, exit loop on error, EOF or EOL */
			n = read(fd, &c, 1);
			if (n != 1 || c == '\n' || c == '\r')
				break;
			*cp++ = c;
		}
		free(readfds);
	} else {
		/* Keep reading until out of space, EOF, error, or newline */
		while (--left && (n = read(fd, &c, 1)) == 1 && c != '\n' && c != '\r')
			*cp++ = c;
	}
	*cp = '\0';

	return (cp == buf ? NULL : buf);
}

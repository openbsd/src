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
 * $OpenBSD: skeylogin.c,v 1.40 2001/12/07 05:09:33 millert Exp $
 */

#include <sys/param.h>
#ifdef	QUOTA
#include <sys/quota.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>

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

static void skey_fakeprompt __P((char *, char *));
static char *tgetline __P((int, char *, size_t, int));

/*
 * Return an skey challenge string for user 'name'. If successful,
 * fill in the caller's skey structure and return(0). If unsuccessful
 * (e.g., if name is unknown) return(-1).
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
		(void)sprintf(ss, "otp-%.*s %d %.*s", SKEY_MAX_HASHNAME_LEN,
			      skey_get_algorithm(), mp->n - 1,
			      SKEY_MAX_SEED_LEN, mp->seed);
		return(0);

	case 1:		/* User not found */
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		/* FALLTHROUGH */

	default:	/* File error */
		skey_fakeprompt(name, ss);
		return(-1);
	}
}

/* 
 * Find an entry in the One-time Password database and lock it.
 *
 * Return codes:
 * -1: error in opening database or unable to lock entry
 *  0: entry found, file R/W pointer positioned at beginning of record
 *  1: entry not found, file R/W pointer positioned at EOF
 */
int
skeylookup(mp, name)
	struct skey *mp;
	char *name;
{
	FILE *keyfile;
	int rval;
	int locked = 0;
	long recstart = 0;
	char *cp, *ht = NULL;
	struct stat statbuf;
	struct flock fl;

	/* Open _PATH_SKEYKEYS if it exists, else return an error */
	if (stat(_PATH_SKEYKEYS, &statbuf) == 0 &&
	    (keyfile = mp->keyfile = fopen(_PATH_SKEYKEYS, "r+")) != NULL) {
		if ((statbuf.st_mode & 0007777) != 0600)
			fchmod(fileno(keyfile), 0600);
	} else {
		mp->keyfile = NULL;
		return(-1);
	}

	/* Look up user name in database */
	while (!feof(keyfile)) {
		mp->recstart = recstart = ftell(keyfile);
		if (fgets(mp->buf, sizeof(mp->buf), keyfile) == NULL)
			break;
		if (mp->buf[0] == '#')
			continue;	/* Comment */
		mp->len = strlen(mp->buf);
		cp = mp->buf + mp->len - 1;
		while (cp >= mp->buf && (*cp == '\n' || *cp == '\r'))
			*cp-- = '\0';
		if ((mp->logname = strtok(mp->buf, " \t")) == NULL ||
		    strcmp(mp->logname, name) != 0)
			continue;
		if ((cp = strtok(NULL, " \t")) == NULL)
			continue;
		/* Save hash type if specified, else use md4 */
		if (isalpha(*cp)) {
			ht = cp;
			if ((cp = strtok(NULL, " \t")) == NULL)
				continue;
		} else {
			ht = "md4";
		}
		mp->n = atoi(cp);
		if ((mp->seed = strtok(NULL, " \t")) == NULL)
			continue;
		if ((mp->val = strtok(NULL, " \t")) == NULL)
			continue;

		/* Set hash type */
		if (ht && skey_set_algorithm(ht) == NULL) {
			warnx("Unknown hash algorithm %s, using %s", ht,
			      skey_get_algorithm());
		}
		(void)fseek(keyfile, recstart, SEEK_SET);

		/* If we already aquired the lock we are done */
		if (locked)
			return(0);

		/* Ortherwise, we must lock the record */
		fl.l_start = mp->recstart;
		fl.l_len = mp->len;
		fl.l_pid = getpid();
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;

		/* If we get the lock on the first try we are done */
		rval = fcntl(fileno(mp->keyfile), F_SETLK, &fl);
		if (rval == 0)
			return(0);
		else if (errno != EAGAIN)
			break;

		/*
		 * Wait until we our lock is granted...
		 * Since we didn't get the lock on the first try, someone
		 * else may have modified the record.  We need to make
		 * sure the entry hasn't changed name (it could have been
		 * commented out) and re-read it.
		 */
		if (fcntl(fileno(mp->keyfile), F_SETLKW, &fl) == -1)
			break;

		rval = fread(mp->logname, fl.l_len, 1, mp->keyfile);
		if (rval != fl.l_len ||
		    memcmp(mp->logname, name, rval) != 0) {
			/* username no longer matches so unlock */
			fl.l_type = F_UNLCK;
			fcntl(fileno(mp->keyfile), F_SETLK, &fl);
		} else {
			locked = 1;
		}
		(void)fseek(keyfile, recstart, SEEK_SET);
	}

	/* No entry found, fill in what we can... */
	memset(mp, 0, sizeof(*mp));
	strlcpy(mp->buf, name, sizeof(mp->buf));
	mp->logname = mp->buf;
	mp->len = strlen(mp->buf);
	mp->keyfile = keyfile;
	mp->recstart = ftell(keyfile);
	return(1);
}

/*
 * Get the next entry in the One-time Password database.
 *
 * Return codes:
 * -1: error in opening database
 *  0: next entry found and stored in mp
 *  1: no more entries, file R/W pointer positioned at EOF
 */
int
skeygetnext(mp)
	struct skey *mp;
{
	int rval;
	int locked = 0;
	char *cp;
	struct stat statbuf;
	struct flock fl;

	/* Open _PATH_SKEYKEYS if it exists, else return an error */
	if (mp->keyfile == NULL) {
		if (stat(_PATH_SKEYKEYS, &statbuf) == 0 &&
		    (mp->keyfile = fopen(_PATH_SKEYKEYS, "r+")) != NULL) {
			if ((statbuf.st_mode & 0007777) != 0600)
				fchmod(fileno(mp->keyfile), 0600);
		} else {
			return(-1);
		}
	} else {
		/* Unlock existing record */
		fl.l_start = mp->recstart;
		fl.l_len = mp->len;
		fl.l_pid = getpid();
		fl.l_type = F_UNLCK;
		fl.l_whence = SEEK_SET;

		fcntl(fileno(mp->keyfile), F_SETLK, &fl);
	}

	/* Look up next user in database */
	while (!feof(mp->keyfile)) {
		mp->recstart = ftell(mp->keyfile);
		if (fgets(mp->buf, sizeof(mp->buf), mp->keyfile) != mp->buf)
			break;
		if (mp->buf[0] == '#')
			continue;	/* Comment */
		mp->len = strlen(mp->buf);
		cp = mp->buf + mp->len - 1;
		while (cp >= mp->buf && (*cp == '\n' || *cp == '\r'))
			*cp-- = '\0';
		if ((mp->logname = strtok(mp->buf, " \t")) == NULL)
			continue;
		if ((cp = strtok(NULL, " \t")) == NULL)
			continue;
		/* Save hash type if specified, else use md4 */
		if (isalpha(*cp)) {
			if ((cp = strtok(NULL, " \t")) == NULL)
				continue;
		}
		mp->n = atoi(cp);
		if ((mp->seed = strtok(NULL, " \t")) == NULL)
			continue;
		if ((mp->val = strtok(NULL, " \t")) == NULL)
			continue;

		/* If we already locked the record, we are done */
		if (locked)
			break;

		/* Got a real entry, lock it */
		fl.l_start = mp->recstart;
		fl.l_len = mp->len;
		fl.l_pid = getpid();
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;

		rval = fcntl(fileno(mp->keyfile), F_SETLK, &fl);
		if (rval == 0)
			break;
		else if (errno != EAGAIN)
			return(-1);

		/*
		 * Someone else has the entry locked, wait
		 * until the lock is free, then re-read the entry.
		 */
		rval = fcntl(fileno(mp->keyfile), F_SETLKW, &fl);
		if (rval == -1)		/* Can't get exclusive lock */
			return(-1);
		locked = 1;
		(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	}
	return(feof(mp->keyfile));
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
	time_t now;
	struct tm *tm;
	struct flock fl;
	char tbuf[27];
	char *cp;
	int len;

	/*
	 * The record should already be locked but lock it again
	 * just to be safe.  We don't wait for the lock to become
	 * available since we should already have it...
	 */
	fl.l_start = mp->recstart;
	fl.l_len = mp->len;
	fl.l_pid = getpid();
	fl.l_type = F_WRLCK;
	fl.l_whence = SEEK_SET;
	if (fcntl(fileno(mp->keyfile), F_SETLK, &fl) != 0) {
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(-1);
	}

	time(&now);
	tm = localtime(&now);
	(void)strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	if (response == NULL) {
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(-1);
	}
	rip(response);

	/* Convert response to binary */
	if (etob(key, response) != 1 && atob8(key, response) != 0) {
		/* Neither english words nor ascii hex */
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(-1);
	}

	/* Compute fkey = f(key) */
	(void)memcpy(fkey, key, sizeof(key));
        (void)fflush(stdout);
	f(fkey);

	/* Reread the file record NOW */
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	if (fgets(mp->buf, sizeof(mp->buf), mp->keyfile) == NULL) {
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(-1);
	}
	len = strlen(mp->buf) - 1;
	cp = mp->buf + len;
	while (cp >= mp->buf && (*cp == '\n' || *cp == '\r'))
		*cp-- = '\0';
	mp->logname = strtok(mp->buf, " \t");
	cp = strtok(NULL, " \t") ;
	if (isalpha(*cp))
		cp = strtok(NULL, " \t") ;
	mp->seed = strtok(NULL, " \t");
	mp->val = strtok(NULL, " \t");
	/* And convert file value to hex for comparison */
	atob8(filekey, mp->val);

	/* Do actual comparison */
	if (memcmp(filekey, fkey, SKEY_BINKEY_SIZE) != 0){
		/* Wrong response */
		(void)fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(1);
	}

	/*
	 * Update key in database by overwriting entire record. Note
	 * that we must write exactly the same number of bytes as in
	 * the original record (note fixed width field for N)
	 */
	btoa8(mp->val,key);
	mp->n--;
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	len -= strlen(mp->logname) + strlen(skey_get_algorithm()) +
	    strlen(mp->val) + strlen(tbuf) + 9;
	/*
	 * If we run out of room it is because we read an old-style
	 * md4 entry without an explicit hash type.
	 */
	if (len < strlen(mp->seed))
		(void)fprintf(mp->keyfile, "%s %04d %-16s %s %-21s\n",
			      mp->logname, mp->n, mp->seed, mp->val, tbuf);
	else
		(void)fprintf(mp->keyfile, "%s %s %04d %-*s %s %-21s\n",
			      mp->logname, skey_get_algorithm(), mp->n,
			      len, mp->seed, mp->val, tbuf);

	(void)fclose(mp->keyfile);
	mp->keyfile = NULL;
	return(0);
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
	return(i);
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
		return(0);

	if (skey.keyfile != NULL) {
		fclose(skey.keyfile);
		skey.keyfile = NULL;
	}
	return(str);
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
		return(-1);

	if (skeyverify(&skey, passwd) == 0)
		return(skey.n);

	return(-1);
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

		(void)sprintf(skeyprompt,
			      "otp-%.*s %d %.*s",
			      SKEY_MAX_HASHNAME_LEN,
			      skey_get_algorithm(),
			      ptr, SKEY_MAX_SEED_LEN,
			      pbuf);
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

		(void)sprintf(skeyprompt, "otp-%.*s %d %.*s",
			      SKEY_MAX_HASHNAME_LEN,
			      skey_get_algorithm(),
			      99, SKEY_MAX_SEED_LEN, pbuf);
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
		return(0);
	}
	return(-1);
}

/*
 * Comment out user's entry in the S/Key database
 *
 * Return codes:
 * -1: Write error; database unchanged
 *  0:  Database updated
 *
 * The database file is always closed by this call.
 */
int
skeyzero(mp)
	struct skey *mp;
{

	/*
	 * Seek to the right place and write comment character
	 * which effectively zero's out the entry.
	 */
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	if (fputc('#', mp->keyfile) == EOF) {
		fclose(mp->keyfile);
		mp->keyfile = NULL;
		return(-1);
	}

	(void)fclose(mp->keyfile);
	mp->keyfile = NULL;
	return(0);
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
	struct flock fl;

	if (mp->logname == NULL || mp->keyfile == NULL)
		return(-1);

	fl.l_start = mp->recstart;
	fl.l_len = mp->len;
	fl.l_pid = getpid();
	fl.l_type = F_UNLCK;
	fl.l_whence = SEEK_SET;

	return(fcntl(fileno(mp->keyfile), F_SETLK, &fl));
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
	    return(NULL);			/* sanity */

    cp = buf;
    left = bufsiz;

    /*
     * Timeout of <= 0 means no timeout.
     */
    if (timeout > 0) {
	    /* Setup for select(2) */
	    n = howmany(fd + 1, NFDBITS) * sizeof(fd_mask);
	    if ((readfds = (fd_set *) malloc(n)) == NULL)
		    return(NULL);
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
			    return(NULL);		/* timeout */
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

    return(cp == buf ? NULL : buf);
}

/* S/KEY v1.1b (skeylogin.c)
 *
 * Authors:
 *          Neil M. Haller <nmh@thumper.bellcore.com>
 *          Philip R. Karn <karn@chicago.qualcomm.com>
 *          John S. Walden <jsw@thumper.bellcore.com>
 *          Scott Chasin <chasin@crimelab.com>
 *
 * S/KEY verification check, lookups, and authentication.
 * 
 * $Id: skeylogin.c,v 1.9 1996/10/14 03:09:13 millert Exp $
 */

#include <sys/param.h>
#ifdef	QUOTA
#include <sys/quota.h>
#endif
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

#include "skey.h"

#ifndef _PATH_KEYFILE
#define	_PATH_KEYFILE	"/etc/skeykeys"
#endif

char *skipspace __P((char *));
int skeylookup __P((struct skey *, char *));

/* Issue a skey challenge for user 'name'. If successful,
 * fill in the caller's skey structure and return 0. If unsuccessful
 * (e.g., if name is unknown) return -1.
 *
 * The file read/write pointer is left at the start of the
 * record.
 */
int
getskeyprompt(mp, name, prompt)
	struct skey *mp;
	char *name;
	char *prompt;
{
	int rval;

	sevenbit(name);
	rval = skeylookup(mp, name);
	(void)strcpy(prompt, "otp-md0 55 latour1\n");
	switch (rval) {
	case -1:	/* File error */
		return -1;
	case 0:		/* Lookup succeeded, return challenge */
		(void)sprintf(prompt, "otp-%s %d %s\n", skey_get_algorithm(),
			      mp->n - 1, mp->seed);
		return 0;
	case 1:		/* User not found */
		(void)fclose(mp->keyfile);
		return -1;
	}
	return -1;	/* Can't happen */
}

/* Return  a skey challenge string for user 'name'. If successful,
 * fill in the caller's skey structure and return 0. If unsuccessful
 * (e.g., if name is unknown) return -1.
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

	rval = skeylookup(mp,name);
	switch(rval){
	case -1:	/* File error */
		return -1;
	case 0:		/* Lookup succeeded, issue challenge */
		(void)sprintf(ss, "otp-%s %d %s", skey_get_algorithm(),
			      mp->n - 1, mp->seed);
		return 0;
	case 1:		/* User not found */
		(void)fclose(mp->keyfile);
		return -1;
	}
	return -1;	/* Can't happen */
}	

/* Find an entry in the One-time Password database.
 * Return codes:
 * -1: error in opening database
 *  0: entry found, file R/W pointer positioned at beginning of record
 *  1: entry not found, file R/W pointer positioned at EOF
 */
int
skeylookup(mp, name)
	struct skey *mp;
	char *name;
{
	int found = 0;
	long recstart = 0;
	char *cp, *ht;
	struct stat statbuf;

	/* See if _PATH_KEYFILE exists, and create it if not */
	if (stat(_PATH_KEYFILE, &statbuf) == -1 && errno == ENOENT) {
		mp->keyfile = fopen(_PATH_KEYFILE, "w+");
		if (mp->keyfile)
			chmod(_PATH_KEYFILE, 0644);
	} else {
		/* Otherwise open normally for update */
		mp->keyfile = fopen(_PATH_KEYFILE, "r+");
	}
	if (mp->keyfile == NULL)
		return -1;

	/* Look up user name in database */
	while (!feof(mp->keyfile)) {
		recstart = ftell(mp->keyfile);
		mp->recstart = recstart;
		if (fgets(mp->buf, sizeof(mp->buf), mp->keyfile) != mp->buf)
			break;
		rip(mp->buf);
		if (mp->buf[0] == '#')
			continue;	/* Comment */
		if ((mp->logname = strtok(mp->buf, " \t")) == NULL)
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
		if (strcmp(mp->logname, name) == 0) {
			found = 1;
			break;
		}
	}
	if (found) {
		(void)fseek(mp->keyfile, recstart, SEEK_SET);
		/* Set hash type */
		if (skey_set_algorithm(ht) == NULL) {
			warnx("Unknown hash algorithm %s, using %s", ht,
			      skey_get_algorithm());
		}
		return 0;
	} else {
		return 1;
	}
}

/* Verify response to a s/key challenge.
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
	char key[8];
	char fkey[8];
	char filekey[8];
	time_t now;
	struct tm *tm;
	char tbuf[27];
	char *cp;

	time(&now);
	tm = localtime(&now);
	(void)strftime(tbuf, sizeof(tbuf), " %b %d,%Y %T", tm);

	if (response == NULL) {
		(void)fclose(mp->keyfile);
		return -1;
	}
	rip(response);

	/* Convert response to binary */
	if (etob(key, response) != 1 && atob8(key, response) != 0) {
		/* Neither english words or ascii hex */
		(void)fclose(mp->keyfile);
		return -1;
	}

	/* Compute fkey = f(key) */
	(void)memcpy(fkey, key, sizeof(key));
        (void)fflush(stdout);
	f(fkey);

	/*
	 * in order to make the window of update as short as possible
	 * we must do the comparison here and if OK write it back
	 * other wise the same password can be used twice to get in
	 * to the system
	 */
	(void)setpriority(PRIO_PROCESS, 0, -4);

	/* reread the file record NOW */
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	if (fgets(mp->buf, sizeof(mp->buf), mp->keyfile) != mp->buf) {
		(void)setpriority(PRIO_PROCESS, 0, 0);
		(void)fclose(mp->keyfile);
		return -1;
	}
	rip(mp->buf);
	mp->logname = strtok(mp->buf, " \t");
	cp = strtok(NULL, " \t") ;
	if (isalpha(*cp))
		cp = strtok(NULL, " \t") ;
	mp->seed = strtok(NULL, " \t");
	mp->val = strtok(NULL, " \t");
	/* And convert file value to hex for comparison */
	atob8(filekey, mp->val);

	/* Do actual comparison */
	if (memcmp(filekey, fkey, 8) != 0){
		/* Wrong response */
		(void)setpriority(PRIO_PROCESS, 0, 0);
		(void)fclose(mp->keyfile);
		return 1;
	}

	/*
	 * Update key in database by overwriting entire record. Note
	 * that we must write exactly the same number of bytes as in
	 * the original record (note fixed width field for N)
	 */
	btoa8(mp->val,key);
	mp->n--;
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	/* Don't save algorithm type for md4 (keep record length same) */
	if (strcmp(skey_get_algorithm(), "md4") == 0)
		(void)fprintf(mp->keyfile, "%s %04d %-16s %s %-21s\n",
			      mp->logname, mp->n, mp->seed, mp->val, tbuf);
	else
		(void)fprintf(mp->keyfile, "%s %s %04d %-16s %s %-21s\n",
			      mp->logname, skey_get_algorithm(), mp->n,
			      mp->seed, mp->val, tbuf);

	(void)fclose(mp->keyfile);
	
	(void)setpriority(PRIO_PROCESS, 0, 0);
	return 0;
}

/*
 * skey_haskey()
 *
 * Returns: 1 user doesnt exist, -1 fle error, 0 user exists.
 *
 */
int
skey_haskey(username)
	char *username;
{
	struct skey skey;
 
	return(skeylookup(&skey, username));
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
	static char str[50];
	struct skey skey;

	i = skeychallenge(&skey, username, str);
	if (i == -1)
		return 0;

	return str;
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
	char *username, *passwd;
{
	int i;
	struct skey skey;

	i = skeylookup(&skey, username);
	if (i == -1 || i == 1)
		return -1;

	if (skeyverify(&skey, passwd) == 0)
		return skey.n;

	return -1;
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
	char pbuf[256], skeyprompt[50];
	struct skey skey;

	/* Attempt an S/Key challenge */
	i = skeychallenge(&skey, username, skeyprompt);

	if (i == -2)
		return 0;

	(void)fprintf(stderr, "%s\n", skeyprompt);
	(void)fflush(stderr);

	(void)fputs("Response: ", stderr);
	readskey(pbuf, sizeof(pbuf));
	rip(pbuf);

	/* Is it a valid response? */
	if (i == 0 && skeyverify(&skey, pbuf) == 0) {
		if (skey.n < 5) {
			(void)fprintf(stderr,
			    "\nWarning! Key initialization needed soon.  (%d logins left)\n",
			    skey.n);
		}
		return 0;
	}
	return -1;
}

/* Comment out user's entry in the s/key database
 *
 * Return codes:
 * -1: Write error; database unchanged
 *  0:  Database updated
 *
 * The database file is always closed by this call.
 */
int
skeyzero(mp, response)
	struct skey *mp;
	char *response;
{
	/*
	 * Seek to the right place and write comment character
	 * which effectively zero's out the entry.
	 */
	(void)fseek(mp->keyfile, mp->recstart, SEEK_SET);
	if (fputc('#', mp->keyfile) == EOF) {
		fclose(mp->keyfile);
		return -1;
	}

	(void)fclose(mp->keyfile);
	
	return 0;
}

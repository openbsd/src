/*	$OpenBSD: database.c,v 1.29 2015/11/09 01:12:27 millert Exp $	*/

/* Copyright 1988,1990,1993,1994 by Paul Vixie
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,2000 by Internet Software Consortium, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <bitstring.h>		/* for structs.h */
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>		/* for structs.h */
#include <unistd.h>

#include "pathnames.h"
#include "macros.h"
#include "structs.h"
#include "funcs.h"

#define HASH(a,b) ((a)+(b))

static	void		process_crontab(const char *, const char *,
					const char *, struct stat *,
					cron_db *, cron_db *);

void
load_database(cron_db **db)
{
	struct stat statbuf, syscron_stat;
	cron_db *new_db, *old_db = *db;
	struct dirent *dp;
	DIR *dir;
	user *u;

	/* before we start loading any data, do a stat on SPOOL_DIR
	 * so that if anything changes as of this moment (i.e., before we've
	 * cached any of the database), we'll see the changes next time.
	 */
	if (stat(SPOOL_DIR, &statbuf) < 0) {
		log_it("CRON", getpid(), "STAT FAILED", SPOOL_DIR);
		return;
	}

	/* track system crontab file
	 */
	if (stat(SYSCRONTAB, &syscron_stat) < 0)
		syscron_stat.st_mtime = 0;

	/* if spooldir's mtime has not changed, we don't need to fiddle with
	 * the database.
	 */
	if (old_db != NULL &&
	    old_db->mtime == HASH(statbuf.st_mtime, syscron_stat.st_mtime)) {
		return;
	}

	/* something's different.  make a new database, moving unchanged
	 * elements from the old database, reloading elements that have
	 * actually changed.  Whatever is left in the old database when
	 * we're done is chaff -- crontabs that disappeared.
	 */
	if ((new_db = malloc(sizeof(*new_db))) == NULL)
		return;
	new_db->mtime = HASH(statbuf.st_mtime, syscron_stat.st_mtime);
	TAILQ_INIT(&new_db->users);

	if (syscron_stat.st_mtime) {
		process_crontab("root", NULL, SYSCRONTAB, &syscron_stat,
				new_db, old_db);
	}

	/* we used to keep this dir open all the time, for the sake of
	 * efficiency.  however, we need to close it in every fork, and
	 * we fork a lot more often than the mtime of the dir changes.
	 */
	if (!(dir = opendir(SPOOL_DIR))) {
		log_it("CRON", getpid(), "OPENDIR FAILED", SPOOL_DIR);
		/* Restore system crontab entry as needed. */
		if (!TAILQ_EMPTY(&new_db->users) &&
		    (u = TAILQ_FIRST(&old_db->users))) {
			if (strcmp(u->name, "*system*") == 0) {
				TAILQ_REMOVE(&old_db->users, u, entries);
				free_user(u);
				TAILQ_INSERT_HEAD(&old_db->users,
				    TAILQ_FIRST(&new_db->users), entries);
			}
		}
		free(new_db);
		return;
	}

	while (NULL != (dp = readdir(dir))) {
		char fname[NAME_MAX+1], tabname[MAX_FNAME];

		/* avoid file names beginning with ".".  this is good
		 * because we would otherwise waste two guaranteed calls
		 * to getpwnam() for . and .., and also because user names
		 * starting with a period are just too nasty to consider.
		 */
		if (dp->d_name[0] == '.')
			continue;

		if (strlcpy(fname, dp->d_name, sizeof fname) >= sizeof fname)
			continue;	/* XXX log? */

		if (snprintf(tabname, sizeof tabname, "%s/%s", SPOOL_DIR, fname) >=
			sizeof(tabname))
			continue;	/* XXX log? */

		process_crontab(fname, fname, tabname,
				&statbuf, new_db, old_db);
	}
	closedir(dir);

	/* if we don't do this, then when our children eventually call
	 * getpwnam() in do_command.c's child_process to verify MAILTO=,
	 * they will screw us up (and v-v).
	 */
	endpwent();

	/* whatever's left in the old database is now junk.
	 */
	if (old_db != NULL) {
		while ((u = TAILQ_FIRST(&old_db->users))) {
			TAILQ_REMOVE(&old_db->users, u, entries);
			free_user(u);
		}
		free(old_db);
	}

	/* overwrite the database control block with the new one.
	 */
	*db = new_db;
}

user *
find_user(cron_db *db, const char *name)
{
	user *u = NULL;

	if (db != NULL) {
		TAILQ_FOREACH(u, &db->users, entries) {
			if (strcmp(u->name, name) == 0)
				break;
		}
	}
	return (u);
}

static void
process_crontab(const char *uname, const char *fname, const char *tabname,
		struct stat *statbuf, cron_db *new_db, cron_db *old_db)
{
	struct passwd *pw = NULL;
	int crontab_fd = -1;
	user *u;

	if (fname == NULL) {
		/* must be set to something for logging purposes.
		 */
		fname = "*system*";
	} else if ((pw = getpwnam(uname)) == NULL) {
		/* file doesn't have a user in passwd file.
		 */
		log_it(fname, getpid(), "ORPHAN", "no passwd entry");
		goto next_crontab;
	}

	if ((crontab_fd = open(tabname, O_RDONLY|O_NONBLOCK|O_NOFOLLOW, 0)) < 0) {
		/* crontab not accessible?
		 */
		log_it(fname, getpid(), "CAN'T OPEN", tabname);
		goto next_crontab;
	}

	if (fstat(crontab_fd, statbuf) < 0) {
		log_it(fname, getpid(), "FSTAT FAILED", tabname);
		goto next_crontab;
	}
	if (!S_ISREG(statbuf->st_mode)) {
		log_it(fname, getpid(), "NOT REGULAR", tabname);
		goto next_crontab;
	}
	if ((statbuf->st_mode & 07577) != 0400) {
		/* Looser permissions on system crontab. */
		if (pw != NULL || (statbuf->st_mode & 022) != 0) {
			log_it(fname, getpid(), "BAD FILE MODE", tabname);
			goto next_crontab;
		}
	}
	if (statbuf->st_uid != 0 && (pw == NULL ||
	    statbuf->st_uid != pw->pw_uid || strcmp(uname, pw->pw_name) != 0)) {
		log_it(fname, getpid(), "WRONG FILE OWNER", tabname);
		goto next_crontab;
	}
	if (pw != NULL && statbuf->st_nlink != 1) {
		log_it(fname, getpid(), "BAD LINK COUNT", tabname);
		goto next_crontab;
	}

	u = find_user(old_db, fname);
	if (u != NULL) {
		/* if crontab has not changed since we last read it
		 * in, then we can just use our existing entry.
		 */
		if (u->mtime == statbuf->st_mtime) {
			TAILQ_REMOVE(&old_db->users, u, entries);
			TAILQ_INSERT_TAIL(&new_db->users, u, entries);
			goto next_crontab;
		}

		/* before we fall through to the code that will reload
		 * the user, let's deallocate and unlink the user in
		 * the old database.  This is more a point of memory
		 * efficiency than anything else, since all leftover
		 * users will be deleted from the old database when
		 * we finish with the crontab...
		 */
		TAILQ_REMOVE(&old_db->users, u, entries);
		free_user(u);
		log_it(fname, getpid(), "RELOAD", tabname);
	}
	u = load_user(crontab_fd, pw, fname);
	if (u != NULL) {
		u->mtime = statbuf->st_mtime;
		TAILQ_INSERT_TAIL(&new_db->users, u, entries);
	}

 next_crontab:
	if (crontab_fd >= 0) {
		close(crontab_fd);
	}
}

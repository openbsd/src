/*
 * Copyright (c) 2004 Bob Beck.  All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include <net/pfvar.h>
#include <arpa/inet.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "grey.h"

extern struct syslog_data sdata;
extern struct passwd *pw;
extern FILE * grey;
extern int debug;

size_t whitecount, whitealloc;
char **whitelist;
int pfdev;

DB		*db;
DBT		dbk, dbd;
BTREEINFO	btreeinfo;

/* borrowed from dhartmei.. */
int
address_valid_v4(const char *a)
{
	if (!*a)
		return (0);
	while (*a)
		if ((*a >= '0' && *a <= '9') || *a == '.')
			a++;
		else
			return (0);
	return (1);
}

int
address_valid_v6(const char *a)
{
	if (!*a)
		return (0);
	while (*a)
		if ((*a >= '0' && *a <= '9') ||
		    (*a >= 'a' && *a <= 'f') ||
		    (*a >= 'A' && *a <= 'F') ||
		    *a == ':')
			a++;
		else
			return (0);
	return (1);
}

int
configure_pf(char **addrs, int count)
{
	static char *argv[11]= {"pfctl", "-p", "/dev/pf", "-q", "-t", 
	    "spamd-white", "-T", "replace", "-f" "-", NULL};
	FILE *pf = NULL;
	int i, pdes[2];
	pid_t pid;
	char *fdpath;

	if (debug)
		fprintf(stderr, "configure_pf - device on fd %d\n", pfdev);
	if (pfdev < 1 || pfdev > 63)
		return(-1);
	if (asprintf(&fdpath, "/dev/fd/%d", pfdev) == -1)
		return(-1);
	argv[2] = fdpath;
	if (pipe(pdes) != 0) {
		syslog_r(LOG_INFO, &sdata, "pipe failed (%m)");
		free(fdpath);
		return(-1);
	}
	switch (pid = fork()) {
	case -1:
		syslog_r(LOG_INFO, &sdata, "fork failed (%m)");
		free(fdpath);
		return(-1);
	case 0:
		/* child */
		close(pdes[1]);
		if (pdes[0] != STDIN_FILENO) {
			dup2(pdes[0], STDIN_FILENO);
			close(pdes[0]);
		}
		execvp(PATH_PFCTL, argv);
		syslog_r(LOG_ERR, &sdata, "can't exec %s:%m", PATH_PFCTL); 
		_exit(1);
	}

	/* parent */
	free(fdpath);
	close(pdes[0]);
	pf = fdopen(pdes[1], "w");
	if (pf == NULL) {
		syslog_r(LOG_INFO, &sdata, "fdopen failed (%m)");
		return(-1);
	}
	for (i = 0; i  < count; i++)
		if (addrs[i] != NULL) {
			fprintf(pf, "%s/32\n", addrs[i]);
			free(addrs[i]);
			addrs[i] = NULL;
		}
	fclose(pf);
	waitpid(pid, NULL, 0);
	return(0);
}

/* validate, then add to list of addrs to whitelist */
int
addwhiteaddr(char *addr)
{
	struct in_addr ia;

	if (address_valid_v4(addr)) {
		if (inet_aton(addr, &ia) == 1) {
			if (whitecount == whitealloc) {
				char **tmp;

				tmp = realloc(whitelist,
				    (whitealloc + 1024) * sizeof(char **));
				if (tmp == NULL)
					return(-1);
				whitelist = tmp;
				whitealloc += 1024;
			}
			whitelist[whitecount] = strdup(addr);
			if (whitelist[whitecount] == NULL)
				return(-1);
			whitecount++;
		}
	} else if (address_valid_v6(addr)) {
		/* XXX deal with v6 later */
		return(-1);
	} else
		return(-1);
	return(0);
}

int
greyscan(char *dbname)
{
	time_t now = time(NULL);
	struct gdata gd;
	int r;

	/* walk db, expire, and whitelist */

	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL) {
		syslog_r(LOG_INFO, &sdata, "dbopen failed (%m)");
		return(-1);
	}
	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
	for (r = db->seq(db, &dbk, &dbd, R_FIRST); !r;
	    r = db->seq(db, &dbk, &dbd, R_NEXT)) {
		char a[128];

		if ((dbk.size < 1) || dbd.size != sizeof(struct gdata)) {
			db->close(db);
			return(-1);
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		if (gd.expire < now) {
			/* get rid of entry */
			if (debug) {
				memset(a, 0, sizeof(a));
				memcpy(a, dbk.data, MIN(sizeof(a),
				    dbk.size));
				syslog_r(LOG_DEBUG, &sdata,
				    "deleting %s from %s", a, dbname);
			}
			if (db->del(db, &dbk, 0)) {
				db->sync(db, 0);
				db->close(db);
				return(-1);
			}
		} else if (gd.pass < now) {
			int tuple = 0;
			char *cp;

			/*
			 * remove this tuple-keyed  entry from db
			 * add address to whitelist
			 * add an address-keyed entry to db
			 */
			memset(a, 0, sizeof(a));
			memcpy(a, dbk.data, MIN(sizeof(a) - 1, dbk.size));
			cp = strchr(a, '\n');
			if (cp != NULL) {
				tuple = 1;
				*cp = '\0';
			}
			if ((addwhiteaddr(a) == -1) && db->del(db, &dbk, 0))
				goto bad;
			if (tuple) {
				if (db->del(db, &dbk, 0))
					goto bad;
				/* re-add entry, keyed only by ip */
				memset(&dbk, 0, sizeof(dbk));
				dbk.size = strlen(a);
				dbk.data = a;
				memset(&dbd, 0, sizeof(dbd));
				dbd.size = sizeof(gd);
				dbd.data = &gd;
				if (db->put(db, &dbk, &dbd, 0))
					goto bad;
				syslog_r(LOG_DEBUG, &sdata,
				    "whitelisting %s in %s", a, dbname);
			}
			if (debug)
				fprintf(stderr, "whitelisted %s\n", a);
		}
	}
	configure_pf(whitelist, whitecount);
	db->sync(db, 0);
	db->close(db);
	return(0);
 bad:
	db->sync(db, 0);
	db->close(db);
	return(-1);
}

int
greyupdate(char *dbname, char *ip, char *from, char *to)
{
	char *key = NULL;
	struct gdata gd;
	time_t now = time(NULL);
	int r;

	/* open with lock, find record, update, close, unlock */
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		return(-1);
	if (asprintf(&key, "%s\n%s\n%s", ip, from, to) == -1)
		goto bad;
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(key);
	dbk.data = key;
	memset(&dbd, 0, sizeof(dbd));
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1)
		goto bad;
	if (r) {
		/* new entry */
		memset(&gd, 0, sizeof(gd));
		gd.first = now;
		gd.bcount = 1;
		gd.pass = now + GREYEXP;
		gd.expire = now + GREYEXP;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(key);
		dbk.data = key;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "added %s\n", key);
	} else {
		/* existing entry */
		if (dbd.size != sizeof(gd)) {
			/* whatever this is, it doesn't belong */
			db->del(db, &dbk, 0);
			goto bad;
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		gd.bcount++;
		if (gd.first + PASSTIME < now) {
			gd.pass = now;
			gd.expire = now + WHITEEXP;
		}
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(key);
		dbk.data = key;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "updated %s\n", key);
	}
	db->close(db);
	free(key);
	return(0);
 bad:
	free(key);
	db->close(db);
	return(-1);
}

int
greyreader(void)
{
	char ip[32], from[MAX_MAIL], to[MAX_MAIL], *buf;
	size_t len;
	int state;

	state = 0;
	if (grey == NULL)
		errx(-1, "No greylist pipe stream!\n");
	while ((buf = fgetln(grey, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else
			/* all valid lines end in \n */
			continue;
		if (strlen(buf) < 4)
			continue;

		switch(state) {
		case 0:
			if (strncmp(buf, "IP:", 3) != 0)
				break;
			strlcpy(ip, buf+3, sizeof(ip));
			if (address_valid_v4(ip))
				state = 1;
			else
				state = 0;
			break;
		case 1:
			if (strncmp(buf, "FR:", 3) != 0) {
				state = 0;
				break;
			}
			strlcpy(from, buf+3, sizeof(from));
			state = 2;
			break;
		case 2:
			if (strncmp(buf, "TO:", 3) != 0) {
				state = 0;
				break;
			}
			strlcpy(to, buf+3, sizeof(to));
			if (debug)
				fprintf(stderr,
				    "Got Grey IP %s from %s to %s\n",
				    ip, from, to);
			greyupdate(PATH_SPAMD_DB, ip, from, to);
			state = 0;
			break;
		}
	}
	return (0);
}

void
greyscanner(void)
{
	for (;;) {
		sleep(DB_SCAN_INTERVAL);
		greyscan(PATH_SPAMD_DB);
	}
	/* NOTREACHED */
}

int
greywatcher(void)
{
	pid_t pid;

	pfdev = open("/dev/pf", O_RDWR);
	if (pfdev == -1)
		err(1, "open of /dev/pf failed");

	/* check to see if /var/db/spamd exists, if not, create it */
	if (open(PATH_SPAMD_DB, O_RDWR, 0) == -1 && errno == ENOENT) {
		int i;
		i = open(PATH_SPAMD_DB, O_RDWR|O_CREAT, 0644);
		if (i == -1)
			err(1, "can't create %s", PATH_SPAMD_DB);
		/* if we are dropping privs, chown to that user */
		if (pw && (fchown(i, pw->pw_uid, pw->pw_gid) == -1))
			err(1, "can't chown %s", PATH_SPAMD_DB);
	}

	/*
	 * lose root, continue as non-root user
	 * XXX Should not be _spamd - as it currently is.
	 */
	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}

	if (!debug) {
		if (daemon(1, 1) == -1)
			err(1, "fork");
	}

	pid = fork();
	if (pid == -1)
		err(1, "fork");
	if (pid == 0) {
		/*
		 * child, talks to jailed spamd over greypipe,
		 * updates db. has no access to pf.
		 */
		close(pfdev);
		setproctitle("(%s update)", PATH_SPAMD_DB); 
		greyreader();
	} else {
		/*
		 * parent, scans db periodically for changes and updates
		 * pf whitelist table accordingly.
		 */
		fclose(grey);
		setproctitle("(pf <spamd-white> update)"); 
		greyscanner();
	}
	return(0);
}

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
#include <time.h>
#include <unistd.h>

#include "grey.h"
#define PATH_SPAMD_DB "/var/db/spamd"

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
static int
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
dbupdate(char *dbname, char *ip, int add)
{
	struct gdata gd;
	time_t now;
	int r;

	now = time(NULL);
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		return(-1);
	if (!address_valid_v4(ip)) {
		warnx("invalid ip address %s\n", ip);
		goto bad;
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(ip);
	dbk.data = ip;
	memset(&dbd, 0, sizeof(dbd));
	if (!add) {
		/* remove whitelist entry */
		r = db->get(db, &dbk, &dbd, 0);
		if (r == -1) {
			warn("db->get failed");
			goto bad;
		}
		if (r) {
			warnx("no entry for %s", ip);
			goto bad;
		} else if (db->del(db, &dbk, 0)) {
			warn("db->del failed");
			goto bad;
		}
	} else {
		/* add or update whitelist entry */
		r = db->get(db, &dbk, &dbd, 0);
		if (r == -1) {
			warn("db->get failed");
			goto bad;
		}
		if (r) {
			/* new entry */
			memset(&gd, 0, sizeof(gd));
			gd.first = now;
			gd.bcount = 1;
			gd.pass = now;
			gd.expire = now + WHITEEXP;
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(ip);
			dbk.data = ip;
			memset(&dbd, 0, sizeof(dbd));
			dbd.size = sizeof(gd);
			dbd.data = &gd;
			r = db->put(db, &dbk, &dbd, 0);
			if (r) {
				warn("db->put failed");
				goto bad;
			}
		} else {
			if (dbd.size != sizeof(gd)) {
				/* whatever this is, it doesn't belong */
				db->del(db, &dbk, 0);
				goto bad;
			}
			memcpy(&gd, dbd.data, sizeof(gd));
			gd.pcount++;
			gd.expire = now + WHITEEXP;
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(ip);
			dbk.data = ip;
			memset(&dbd, 0, sizeof(dbd));
			dbd.size = sizeof(gd);
			dbd.data = &gd;
			r = db->put(db, &dbk, &dbd, 0);
			if (r) {
				warn("db->put failed");
				goto bad;
			}
		}
	}
	db->sync(db, 0);
	db->close(db);
	return (0);
 bad:
	db->sync(db, 0);
	db->close(db);
	return(-1);
}



int
dblist(char *dbname)
{
	struct gdata gd;
	int r;

	/* walk db, list in text format */
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		err(1, "dbopen");
	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
	for (r = db->seq(db, &dbk, &dbd, R_FIRST); !r;
	    r = db->seq(db, &dbk, &dbd, R_NEXT)) {
		char *a, *cp;

		if ((dbk.size < 1) || dbd.size != sizeof(struct gdata)) {
			db->close(db);
			errx(1, "bogus size db entry - bad db file?");
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		a = malloc(dbk.size + 1);
		if (a == NULL)
			err(1, "malloc");
		memcpy(a, dbk.data, dbk.size);
		a[dbk.size]='\0';
		cp = strchr(a, '\n');
		if (cp == NULL)
			/* this is a whitelist entry */
			printf("WHITE:%s:%d:%d:%d:%d:%d\n", a, gd.first,
			    gd.pass, gd.expire, gd.bcount, gd.pcount);
		else {
			char *from, *to;

			/* greylist entry */
			*cp = '\0';
			from = cp + 1;
			to = strchr(from, '\n');
			if (to == NULL) {
				warnx("No from part in grey key %s", a);
				goto bad;
			}
			*to = '\0';
			to++;
			printf("GREY:%s:%s:%s:%d:%d:%d:%d:%d\n",
			    a, from, to, gd.first, gd.pass, gd.expire,
			    gd.bcount, gd.pcount);
		}
	}
	db->sync(db, 0);
	db->close(db);
	return(0);
 bad:
	db->sync(db, 0);
	db->close(db);
	errx(1, "incorrect db format entry");
	/* NOTREACHED */
	return(-1);
}


static int
usage(void)
{
	fprintf(stderr, "usage: spamdb [-a ip] [-d ip]\n");
	exit(-1);
}

int
main(int argc, char **argv)
{
	int ch, action = 0;
	char *ip = NULL;

	while ((ch = getopt(argc, argv, "a:d:")) != -1) {
		switch (ch) {
		case 'a':
			action = 1;
			ip = optarg;
			break;
		case 'd':
			action = 2;
			ip = optarg;
			break;
		default:
			usage();
			break;
		}
	}

	switch (action) {
	case 0:
		dblist("/var/db/spamd");
		break;
	case 1:
		dbupdate("/var/db/spamd", ip, 1);
		break;
	case 2:
		dbupdate("/var/db/spamd", ip, 0);
		break;
	default:
		errx(-1, "bad action");
	}
	return(0);
}

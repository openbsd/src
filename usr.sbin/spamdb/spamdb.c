/*	$OpenBSD: spamdb.c,v 1.11 2004/04/27 21:25:11 itojun Exp $	*/

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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <db.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>

#include "grey.h"

int
dbupdate(char *dbname, char *ip, int add)
{
	BTREEINFO	btreeinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	time_t		now;
	int		r;
	struct addrinfo hints, *res;

	now = time(NULL);
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		err(1, "cannot open %s for writing", dbname);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(ip, NULL, &hints, &res) != 0) {
		warnx("invalid ip address %s", ip);
		goto bad;
	}
	freeaddrinfo(res);
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
	db->close(db);
	db = NULL;
	return (0);
 bad:
	db->close(db);
	db = NULL;
	return (1);
}

int
dblist(char *dbname)
{
	BTREEINFO	btreeinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	int		r;

	/* walk db, list in text format */
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDONLY, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		err(1, "cannot open %s for reading", dbname);
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
			printf("WHITE|%s|||%d|%d|%d|%d|%d\n", a, gd.first,
			    gd.pass, gd.expire, gd.bcount, gd.pcount);
		else {
			char *from, *to;

			/* greylist entry */
			*cp = '\0';
			from = cp + 1;
			to = strchr(from, '\n');
			if (to == NULL) {
				warnx("No from part in grey key %s", a);
				free(a);
				goto bad;
			}
			*to = '\0';
			to++;
			printf("GREY|%s|%s|%s|%d|%d|%d|%d|%d\n",
			    a, from, to, gd.first, gd.pass, gd.expire,
			    gd.bcount, gd.pcount);
		}
		free(a);
	}
	db->close(db);
	db = NULL;
	return (0);
 bad:
	db->close(db);
	db = NULL;
	errx(1, "incorrect db format entry");
	/* NOTREACHED */
	return (1);
}

extern char *__progname;

static int
usage(void)
{
	fprintf(stderr, "usage: %s [-a ip] [-d ip]\n", __progname);
	exit(1);
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
		return dblist(PATH_SPAMD_DB);
	case 1:
		return dbupdate(PATH_SPAMD_DB, ip, 1);
	case 2:
		return dbupdate(PATH_SPAMD_DB, ip, 0);
	default:
		errx(-1, "bad action");
	}
	/* NOT REACHED */
	return (0);
}

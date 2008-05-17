/*	$OpenBSD: spamdb.c,v 1.24 2008/05/17 10:48:06 millert Exp $	*/

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
#include <ctype.h>
#include <errno.h>

#include "grey.h"

/* things we may add/delete from the db */
#define WHITE 0
#define TRAPHIT 1
#define SPAMTRAP 2

int	dblist(DB *);
int	dbupdate(DB *, char *, int, int);

int
dbupdate(DB *db, char *ip, int add, int type)
{
	DBT		dbk, dbd;
	struct gdata	gd;
	time_t		now;
	int		r;
	struct addrinfo hints, *res;

	now = time(NULL);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (add && (type == TRAPHIT || type == WHITE)) {
		if (getaddrinfo(ip, NULL, &hints, &res) != 0) {
			warnx("invalid ip address %s", ip);
			goto bad;
		}
		freeaddrinfo(res);
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(ip);
	dbk.data = ip;
	memset(&dbd, 0, sizeof(dbd));
	if (!add) {
		/* remove entry */
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
		/* add or update entry */
		r = db->get(db, &dbk, &dbd, 0);
		if (r == -1) {
			warn("db->get failed");
			goto bad;
		}
		if (r) {
			int i;

			/* new entry */
			memset(&gd, 0, sizeof(gd));
			gd.first = now;
			gd.bcount = 1;
			switch (type) {
			case WHITE:
				gd.pass = now;
				gd.expire = now + WHITEEXP;
				break;
			case TRAPHIT:
				gd.expire = now + TRAPEXP;
				gd.pcount = -1;
				break;
			case SPAMTRAP:
				gd.expire = 0;
				gd.pcount = -2;
				/* ensure address is of the form user@host */
				if (strchr(ip, '@') == NULL)
					errx(-1, "not an email address: %s", ip);
				/* ensure address is lower case*/
				for (i = 0; ip[i] != '\0'; i++)
					if (isupper(ip[i]))
						ip[i] = (char)tolower(ip[i]);
				break;
			default:
				errx(-1, "unknown type %d", type);
			}
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
			switch (type) {
			case WHITE:
				gd.pass = now;
				gd.expire = now + WHITEEXP;
				break;
			case TRAPHIT:
				gd.expire = now + TRAPEXP;
				gd.pcount = -1;
				break;
			case SPAMTRAP:
				gd.expire = 0; /* XXX */
				gd.pcount = -2;
				break;
			default:
				errx(-1, "unknown type %d", type);
			}

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
	return (0);
 bad:
	return (1);
}

int
dblist(DB *db)
{
	DBT		dbk, dbd;
	struct gdata	gd;
	int		r;

	/* walk db, list in text format */
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
		if (cp == NULL) {
			/* this is a non-greylist entry */
			switch (gd.pcount) {
			case -1: /* spamtrap hit, with expiry time */
				printf("TRAPPED|%s|%d\n", a, gd.expire);
				break;
			case -2: /* spamtrap address */
				printf("SPAMTRAP|%s\n", a);
				break;
			default: /* whitelist */
				printf("WHITE|%s|||%d|%d|%d|%d|%d\n", a,
				    gd.first, gd.pass, gd.expire, gd.bcount,
				    gd.pcount);
				break;
			}
		} else {
			char *helo, *from, *to;

			/* greylist entry */
			*cp = '\0';
			helo = cp + 1;
			from = strchr(helo, '\n');
			if (from == NULL) {
				warnx("No from part in grey key %s", a);
				free(a);
				goto bad;
			}
			*from = '\0';
			from++;
			to = strchr(from, '\n');
			if (to == NULL) {
				/* probably old format - print it the
				 * with an empty HELO field instead 
				 * of erroring out.
				 */			  
				printf("GREY|%s|%s|%s|%s|%d|%d|%d|%d|%d\n",
				    a, "", helo, from, gd.first, gd.pass,
				    gd.expire, gd.bcount, gd.pcount);
			
			} else {
				*to = '\0';
				to++;
				printf("GREY|%s|%s|%s|%s|%d|%d|%d|%d|%d\n",
				    a, helo, from, to, gd.first, gd.pass,
				    gd.expire, gd.bcount, gd.pcount);
			}
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
	fprintf(stderr, "usage: %s [[-Tt] -a keys] [[-Tt] -d keys]\n", __progname);
	exit(1);
	/* NOTREACHED */
}

int
main(int argc, char **argv)
{
	int i, ch, action = 0, type = WHITE, r = 0, c = 0;
	HASHINFO	hashinfo;
	DB		*db;

	while ((ch = getopt(argc, argv, "adtT")) != -1) {
		switch (ch) {
		case 'a':
			action = 1;
			break;
		case 'd':
			action = 2;
			break;
		case 't':
			type = TRAPHIT;
			break;
		case 'T':
			type = SPAMTRAP;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (action == 0 && type != WHITE)
		usage();
	
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(PATH_SPAMD_DB, O_EXLOCK | (action ? O_RDWR : O_RDONLY),
	    0600, DB_HASH, &hashinfo);
	if (db == NULL) {
		if (errno == EFTYPE)	
			err(1,
			    "%s is old, run current spamd to convert it",
			    PATH_SPAMD_DB);
		else 
			err(1, "cannot open %s for %s", PATH_SPAMD_DB,
			    action ? "writing" : "reading");
	}

	switch (action) {
	case 0:
		return dblist(db);
	case 1:
		for (i=0; i<argc; i++)
			if (argv[i][0] != '\0') {
				c++;
				r += dbupdate(db, argv[i], 1, type);
			}
		if (c == 0)
			errx(2, "no addresses specified");
		break;
	case 2:
		for (i=0; i<argc; i++)
			if (argv[i][0] != '\0') {
				c++;
				r += dbupdate(db, argv[i], 0, type);
			}
		if (c == 0)
			errx(2, "no addresses specified");
		break;
	default:
		errx(-1, "bad action");
	}
	db->close(db);
	return (r);
}

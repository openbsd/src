/*	$OpenBSD: spamlogd.c,v 1.10 2004/09/16 05:35:02 deraadt Exp $	*/

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

/* watch pf log for mail connections, update whitelist entries. */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <db.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>

#include "grey.h"
#define PATH_TCPDUMP "/usr/sbin/tcpdump"

struct syslog_data sdata = SYSLOG_DATA_INIT;
int inbound; /* do we only whitelist inbound smtp? */

extern char *__progname;

int
dbupdate(char *dbname, char *ip)
{
	BTREEINFO	btreeinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	time_t		now;
	int		r;
	struct in_addr	ia;

	now = time(NULL);
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_BTREE, &btreeinfo);
	if (db == NULL)
		return(-1);
	if (inet_pton(AF_INET, ip, &ia) != 1) {
		syslog_r(LOG_NOTICE, &sdata, "invalid ip address %s", ip);
		goto bad;
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(ip);
	dbk.data = ip;
	memset(&dbd, 0, sizeof(dbd));
	/* add or update whitelist entry */
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1) {
		syslog_r(LOG_NOTICE, &sdata, "db->get failed (%m)");
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
			syslog_r(LOG_NOTICE, &sdata, "db->put failed (%m)");
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
			syslog_r(LOG_NOTICE, &sdata, "db->put failed (%m)");
			goto bad;
		}
	}
	db->close(db);
	db = NULL;
	return (0);
 bad:
	db->close(db);
	db = NULL;
	return (-1);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-I] [-i interface]\n", __progname);
	exit(1);
}

char *targv[17] = {
	"tcpdump", "-l",  "-n", "-e", "-i", "pflog0", "-q",
	"-t", "port", "25", "and", "action", "pass",
	NULL, NULL, NULL, NULL
};

int
main(int argc, char **argv)
{
	int ch, p[2];
	char *buf, *lbuf;
	size_t len;
	FILE *f;


	while ((ch = getopt(argc, argv, "i:I")) != -1) {
		switch (ch) {
		case 'i':
			if (targv[15])	/* may only set once */
				usage();
			targv[13] = "and";
			targv[14] = "on";
			targv[15] = optarg;
			break;
		case 'I':
			inbound = 1;
			break;
		default:
			usage();
			break;
		}
	}

	if (daemon(1, 1) == -1)
		err(1, "daemon");
	if (pipe(p) == -1)
		err(1, "pipe");
	switch (fork()) {
	case -1:
		err(1, "fork");
	case 0:
		/* child */
		close(p[0]);
		close(STDERR_FILENO);
		if (dup2(p[1], STDOUT_FILENO) == -1) {
			warn("dup2");
			_exit(1);
		}
		close(p[1]);
		execvp(PATH_TCPDUMP, targv);
		warn("exec of %s failed", PATH_TCPDUMP);
		_exit(1);
	}

	/* parent */
	close(p[1]);
	f = fdopen(p[0], "r");
	if (f == NULL)
		err(1, "fdopen");
	tzset();
	openlog_r("spamlogd", LOG_PID | LOG_NDELAY, LOG_DAEMON, &sdata);

	lbuf = NULL;
	while ((buf = fgetln(f, &len))) {
		char *cp = NULL;
		char *buf2;

		if ((buf2 = malloc(len + 1)) == NULL) {
			syslog_r(LOG_ERR, &sdata, "malloc failed");
			exit(1);
		}

		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = (char *)malloc(len + 1)) == NULL) {
				syslog_r(LOG_ERR, &sdata, "malloc failed");
				exit(1);
			}
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		if (!inbound && strstr(buf, "pass out") != NULL) {
			/*
			 * this is outbound traffic - we whitelist
			 * the destination address, because we assume
			 * that a reply may come to this outgoing mail
			 * we are sending.
			 */
			if ((cp = (strchr(buf, '>'))) != NULL) {
				if (sscanf(cp, "> %s", buf2) == 1) {
					cp = strrchr(buf2, '.');
					if (cp != NULL) {
						*cp = '\0';
						cp = buf2;
						syslog_r(LOG_DEBUG, &sdata,
						    "outbound %s\n", cp);
					}
				} else
					cp = NULL;
			}

		} else {
			/*
			 * this is inbound traffic - we whitelist
			 * the source address, because this is
			 * traffic presumably to our real MTA
			 */
			if ((cp = (strchr(buf, '>'))) != NULL) {
				while (*cp != '.' && cp >= buf) {
					*cp = '\0';
					cp--;
				}
				*cp ='\0';
				while (*cp != ' ' && cp >= buf)
					cp--;
				cp++;
				syslog_r(LOG_DEBUG, &sdata,
				    "inbound %s\n", cp);
			}
		}
		if (cp != NULL)
			dbupdate(PATH_SPAMD_DB, cp);

		free(lbuf);
		lbuf = NULL;
		free(buf2);
	}
	exit(0);
}

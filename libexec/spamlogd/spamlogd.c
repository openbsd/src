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
#define PATH_TCPDUMP "/usr/sbin/tcpdump"

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
dbupdate(char *dbname, char *ip)
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
	db->sync(db, 0);
	db->close(db);
	return (0);
 bad:
	db->sync(db, 0);
	db->close(db);
	return(-1);
}

static int
usage(void)
{
	fprintf(stderr, "usage: spamlogd [-i netif]\n");
	exit(-1);
}

char *targv[19] = {
	"tcpdump", "-l",  "-n", "-e", "-i", "pflog0", "-q",
	"-t", "inbound", "and", "port", "25", "and", "action", "pass",
	NULL, NULL, NULL, NULL
};

int
main(int argc, char **argv)
{
	int ch, p[2];
	char *buf, *lbuf;
	size_t len;
	pid_t pid;
	FILE *f;

	while ((ch = getopt(argc, argv, "i:")) != -1) {
		switch (ch) {
		case 'i':
			targv[15] = "and";
			targv[16] = "on";
			targv[17] = optarg;
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
	switch (pid = fork()) {
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
	f = fdopen(p[0], "r");
	if (f == NULL)
		err(1, "fdopen");
	lbuf = NULL;
	while ((buf = fgetln(f, &len))) {
		char *cp;

		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = (char *)malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}
		if ((cp = (strchr(buf, '>'))) != NULL) {
			/* XXX replace this grot with an sscanf */
			while (*cp != '.' && cp >= buf) {
				*cp = '\0';
				cp--;
			}
			*cp ='\0';
			while (*cp != ' ' && cp >= buf)
				cp--;
			cp++;
			dbupdate(PATH_SPAMD_DB, cp);
		}
		if (lbuf != NULL) {
			free(lbuf);
			lbuf = NULL;
		}
	}
	exit(0);
}

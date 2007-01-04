/*	$OpenBSD: grey.c,v 1.24 2007/01/04 21:41:37 beck Exp $	*/

/*
 * Copyright (c) 2004-2006 Bob Beck.  All rights reserved.
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
#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <netdb.h>

#include "grey.h"

extern time_t passtime, greyexp, whiteexp, trapexp;
extern struct syslog_data sdata;
extern struct passwd *pw;
extern u_short cfg_port;
extern pid_t jail_pid;
extern FILE * trapcfg;
extern FILE * grey;
extern int debug;

size_t whitecount, whitealloc;
size_t trapcount, trapalloc;
char **whitelist;
char **traplist;

char *traplist_name = "spamd-greytrap";
char *traplist_msg = "\"Your address %A has mailed to spamtraps here\\n\"";

pid_t db_pid = -1;
int pfdev;
int spamdconf;

struct db_change {
	SLIST_ENTRY(db_change)	entry;
	char *			key;
	void *			data;
	size_t			dsiz;
	int			act;
};

#define DBC_ADD 1
#define DBC_DEL 2

/* db pending changes list */
SLIST_HEAD(,  db_change) db_changes = SLIST_HEAD_INITIALIZER(db_changes);

static char *pargv[11]= {
	"pfctl", "-p", "/dev/pf", "-q", "-t",
	"spamd-white", "-T", "replace", "-f" "-", NULL
};

/* If the parent gets a signal, kill off the children and exit */
/* ARGSUSED */
static void
sig_term_chld(int sig)
{
	if (db_pid != -1)
		kill(db_pid, SIGTERM);
	if (jail_pid != -1)
		kill(jail_pid, SIGTERM);
	_exit(1);
}

/*
 * Greatly simplified version from spamd_setup.c  - only
 * sends one blacklist to an already open stream. Has no need
 * to collapse cidr ranges since these are only ever single
 * host hits.
 */
int
configure_spamd(char **addrs, int count, FILE *sdc)
{
	int i;

	fprintf(sdc, "%s;%s;", traplist_name, traplist_msg);
	for (i = 0; i < count; i++)
		fprintf(sdc, "%s/32;", addrs[i]);
	fprintf(sdc, "\n");
	if (fflush(sdc) == EOF)
		syslog_r(LOG_DEBUG, &sdata, "configure_spamd: fflush failed (%m)");
	return(0);
}

int
configure_pf(char **addrs, int count)
{
	FILE *pf = NULL;
	int i, pdes[2], status;
	pid_t pid;
	char *fdpath;
	struct sigaction sa;

	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_term_chld;

	if (debug)
		fprintf(stderr, "configure_pf - device on fd %d\n", pfdev);
	if (pfdev < 1 || pfdev > 63)
		return(-1);
	if (asprintf(&fdpath, "/dev/fd/%d", pfdev) == -1)
		return(-1);
	pargv[2] = fdpath;
	if (pipe(pdes) != 0) {
		syslog_r(LOG_INFO, &sdata, "pipe failed (%m)");
		free(fdpath);
		fdpath = NULL;
		return(-1);
	}
	signal(SIGCHLD, SIG_DFL);
	switch (pid = fork()) {
	case -1:
		syslog_r(LOG_INFO, &sdata, "fork failed (%m)");
		free(fdpath);
		fdpath = NULL;
		close(pdes[0]);
		close(pdes[1]);
		sigaction(SIGCHLD, &sa, NULL);
		return(-1);
	case 0:
		/* child */
		close(pdes[1]);
		if (pdes[0] != STDIN_FILENO) {
			dup2(pdes[0], STDIN_FILENO);
			close(pdes[0]);
		}
		execvp(PATH_PFCTL, pargv);
		syslog_r(LOG_ERR, &sdata, "can't exec %s:%m", PATH_PFCTL);
		_exit(1);
	}

	/* parent */
	free(fdpath);
	fdpath = NULL;
	close(pdes[0]);
	pf = fdopen(pdes[1], "w");
	if (pf == NULL) {
		syslog_r(LOG_INFO, &sdata, "fdopen failed (%m)");
		close(pdes[1]);
		sigaction(SIGCHLD, &sa, NULL);
		return(-1);
	}
	for (i = 0; i < count; i++)
		if (addrs[i] != NULL)
			fprintf(pf, "%s/32\n", addrs[i]);
	fclose(pf);

	waitpid(pid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		syslog_r(LOG_ERR, &sdata, "%s returned status %d", PATH_PFCTL,
		    WEXITSTATUS(status));
	else if (WIFSIGNALED(status))
		syslog_r(LOG_ERR, &sdata, "%s died on signal %d", PATH_PFCTL,
		    WTERMSIG(status));

	sigaction(SIGCHLD, &sa, NULL);
	return(0);
}

void
freeaddrlists(void)
{
	int i;

	if (whitelist != NULL)
		for (i = 0; i < whitecount; i++) {
			free(whitelist[i]);
			whitelist[i] = NULL;
		}
	whitecount = 0;
	if (traplist != NULL) {
		for (i = 0; i < trapcount; i++) {
			free(traplist[i]);
			traplist[i] = NULL;
		}
	}
	trapcount = 0;
}

/* validate, then add to list of addrs to whitelist */
int
addwhiteaddr(char *addr)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(addr, NULL, &hints, &res) == 0) {
		if (whitecount == whitealloc) {
			char **tmp;

			tmp = realloc(whitelist,
			    (whitealloc + 1024) * sizeof(char *));
			if (tmp == NULL) {
				freeaddrinfo(res);
				return(-1);
			}
			whitelist = tmp;
			whitealloc += 1024;
		}
		whitelist[whitecount] = strdup(addr);
		if (whitelist[whitecount] == NULL) {
			freeaddrinfo(res);
			return(-1);
		}
		whitecount++;
		freeaddrinfo(res);
	} else
		return(-1);
	return(0);
}

/* validate, then add to list of addrs to traplist */
int
addtrapaddr(char *addr)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	if (getaddrinfo(addr, NULL, &hints, &res) == 0) {
		if (trapcount == trapalloc) {
			char **tmp;

			tmp = realloc(traplist,
			    (trapalloc + 1024) * sizeof(char *));
			if (tmp == NULL) {
				freeaddrinfo(res);
				return(-1);
			}
			traplist = tmp;
			trapalloc += 1024;
		}
		traplist[trapcount] = strdup(addr);
		if (traplist[trapcount] == NULL) {
			freeaddrinfo(res);
			return(-1);
		}
		trapcount++;
		freeaddrinfo(res);
	} else
		return(-1);
	return(0);
}

static int
queue_change(char *key, char *data, size_t dsiz, int act) {
	struct db_change *dbc;

	if ((dbc = malloc(sizeof(*dbc))) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		return(-1);
	}
	if ((dbc->key = strdup(key)) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		free(dbc);
		return(-1);
	}
	if ((dbc->data = malloc(dsiz)) == NULL) {
		syslog_r(LOG_DEBUG, &sdata, "malloc failed (queue change)");
		free(dbc->key);
		free(dbc);
		return(-1);
	}
	memcpy(dbc->data, data, dsiz);
	dbc->dsiz = dsiz;
	dbc->act = act;
	syslog_r(LOG_DEBUG, &sdata,
	    "queueing %s of %s", ((act == DBC_ADD) ? "add" : "deletion"),
	     dbc->key);
	SLIST_INSERT_HEAD(&db_changes, dbc, entry);
	return(0);
}	

static int
do_changes(DB *db) {
	DBT			dbk, dbd;	 		
	struct db_change	*dbc;
	int ret = 0;

	while (!SLIST_EMPTY(&db_changes)) {
		dbc = SLIST_FIRST(&db_changes);
		switch(dbc->act) {
		case DBC_ADD:
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(dbc->key);
			dbk.data = dbc->key;
			memset(&dbd, 0, sizeof(dbd));
			dbd.size = dbc->dsiz;
			dbd.data = dbc->data;
			if (db->put(db, &dbk, &dbd, 0)) {
				db->sync(db, 0);
				syslog_r(LOG_ERR, &sdata,
				    "can't add %s to spamd db (%m)", dbc->key);
				ret = -1;
			}
			db->sync(db, 0);
			break;
		case DBC_DEL:
			memset(&dbk, 0, sizeof(dbk));
			dbk.size = strlen(dbc->key);
			dbk.data = dbc->key;
			if (db->del(db, &dbk, 0)) {
				syslog_r(LOG_ERR, &sdata,
				    "can't delete %s from spamd db (%m)",
				    dbc->key);
				ret = -1;
			}
			break;
		default:
			syslog_r(LOG_ERR, &sdata, "Unrecognized db change");
			ret = -1;
		}
		free(dbc->key);
		dbc->key = NULL;
		free(dbc->data);
		dbc->data = NULL;
		dbc->act = 0;
		dbc->dsiz = 0;
		SLIST_REMOVE_HEAD(&db_changes, entry);

	}
	return(ret);
}	


int
greyscan(char *dbname)
{
	HASHINFO	hashinfo;
	DBT		dbk, dbd;
	DB		*db;
	struct gdata	gd;
	int		r;
	char		*a = NULL;
	size_t		asiz = 0;
	time_t now = time(NULL);

	/* walk db, expire, and whitelist */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db == NULL) {
		syslog_r(LOG_INFO, &sdata, "dbopen failed (%m)");
		return(-1);
	}
	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
	for (r = db->seq(db, &dbk, &dbd, R_FIRST); !r;
	    r = db->seq(db, &dbk, &dbd, R_NEXT)) {
		if ((dbk.size < 1) || dbd.size != sizeof(struct gdata)) {
			syslog_r(LOG_ERR, &sdata, "bogus entry in spamd database");
			goto bad;
		}
		if (asiz < dbk.size + 1) {
			char *tmp;

			tmp = realloc(a, dbk.size * 2);
			if (tmp == NULL)
				goto bad;
			a = tmp;
			asiz = dbk.size * 2;
		}
		memset(a, 0, asiz);
		memcpy(a, dbk.data, dbk.size);
		memcpy(&gd, dbd.data, sizeof(gd));
		if (gd.expire <= now && gd.pcount != -2) {
			/* get rid of entry */
			if (queue_change(a, NULL, 0, DBC_DEL) == -1)
				goto bad;
		} else if (gd.pcount == -1)  {
			/* this is a greytrap hit */
			if ((addtrapaddr(a) == -1) &&
			    (queue_change(a, NULL, 0, DBC_DEL) == -1))
				goto bad;
		} else if (gd.pcount >= 0 && gd.pass <= now) {
			int tuple = 0;
			char *cp;

			/*
			 * remove this tuple-keyed  entry from db
			 * add address to whitelist
			 * add an address-keyed entry to db
			 */

			cp = strchr(a, '\n');
			if (cp != NULL) {
				tuple = 1;
				*cp = '\0';
			}

			if (addwhiteaddr(a) == -1) {
				if (cp != NULL)
					*cp = '\n';
				if (queue_change(a, NULL, 0, DBC_DEL) == -1)
					goto bad;
			}

			if (tuple) {
				if (cp != NULL)
					*cp = '\n';
				if (queue_change(a, NULL, 0, DBC_DEL) == -1)
					goto bad;
				if (cp != NULL)
					*cp = '\0';
				/* re-add entry, keyed only by ip */
				gd.expire = now + whiteexp;
 				dbd.size = sizeof(gd);
				dbd.data = &gd;
				if (queue_change(a, (void *) &gd, sizeof(gd),
				    DBC_ADD) == -1)
					goto bad;
				syslog_r(LOG_DEBUG, &sdata,
				    "whitelisting %s in %s", a, dbname);

			}
			if (debug)
				fprintf(stderr, "whitelisted %s\n", a);
		}
	}
	(void) do_changes(db);
	db->close(db);
	db = NULL;
	configure_pf(whitelist, whitecount);
	if (configure_spamd(traplist, trapcount, trapcfg) == -1)
		syslog_r(LOG_DEBUG, &sdata, "configure_spamd failed");

	freeaddrlists();
	free(a);
	a = NULL;
	asiz = 0;
	return(0);
 bad:
	(void) do_changes(db);
	db->close(db);
	db = NULL;
	freeaddrlists();
	free(a);
	a = NULL;
	asiz = 0;
	return(-1);
}

int
greyupdate(char *dbname, char *ip, char *from, char *to)
{
	HASHINFO	hashinfo;
	DBT		dbk, dbd;
	DB		*db;
	char		*key = NULL;
	char		*trap = NULL;
	char		*lookup;
	struct gdata	gd;
	time_t		now, expire;
	int		i, r, spamtrap;

	now = time(NULL);

	/* open with lock, find record, update, close, unlock */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(dbname, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db == NULL)
		return(-1);
	if (asprintf(&key, "%s\n%s\n%s", ip, from, to) == -1)
		goto bad;
	if ((trap = strdup(to)) == NULL)
		goto bad;
	for (i = 0; trap[i] != '\0'; i++)
		if (isupper(trap[i]))
			trap[i] = tolower(trap[i]);
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(trap);
	dbk.data = trap;
	memset(&dbd, 0, sizeof(dbd));
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1)
		goto bad;
	if (r) {
		/* didn't exist - so this doesn't match a known spamtrap  */
		spamtrap = 0;
		lookup = key;
		expire = greyexp;
	} else {
		/* To: address is a spamtrap, so add as a greytrap entry */
		spamtrap = 1;
		lookup = ip;
		expire = trapexp;
	}
	memset(&dbk, 0, sizeof(dbk));
	dbk.size = strlen(lookup);
	dbk.data = lookup;
	memset(&dbd, 0, sizeof(dbd));
	r = db->get(db, &dbk, &dbd, 0);
	if (r == -1)
		goto bad;
	if (r) {
		/* new entry */
		memset(&gd, 0, sizeof(gd));
		gd.first = now;
		gd.bcount = 1;
		gd.pcount = spamtrap ? -1 : 0;
		gd.pass = now + expire;
		gd.expire = now + expire;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(lookup);
		dbk.data = lookup;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "added %s %s\n",
			    spamtrap ? "greytrap entry for" : "", lookup);
	} else {
		/* existing entry */
		if (dbd.size != sizeof(gd)) {
			/* whatever this is, it doesn't belong */
			db->del(db, &dbk, 0);
			db->sync(db, 0);
			goto bad;
		}
		memcpy(&gd, dbd.data, sizeof(gd));
		gd.bcount++;
		gd.pcount = spamtrap ? -1 : 0;
		if (gd.first + passtime < now)
			gd.pass = now;
		memset(&dbk, 0, sizeof(dbk));
		dbk.size = strlen(lookup);
		dbk.data = lookup;
		memset(&dbd, 0, sizeof(dbd));
		dbd.size = sizeof(gd);
		dbd.data = &gd;
		r = db->put(db, &dbk, &dbd, 0);
		db->sync(db, 0);
		if (r)
			goto bad;
		if (debug)
			fprintf(stderr, "updated %s\n", lookup);
	}
	free(key);
	key = NULL;
	free(trap);
	trap = NULL;
	db->close(db);
	db = NULL;
	return(0);
 bad:
	free(key);
	key = NULL;
	free(trap);
	trap = NULL;
	db->close(db);
	db = NULL;
	return(-1);
}

int
greyreader(void)
{
	char ip[32], from[MAX_MAIL], to[MAX_MAIL], *buf;
	size_t len;
	int state;
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;		/*for now*/
	hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
	hints.ai_protocol = IPPROTO_UDP;	/*dummy*/
	hints.ai_flags = AI_NUMERICHOST;

	state = 0;
	if (grey == NULL) {
		syslog_r(LOG_ERR, &sdata, "No greylist pipe stream!\n");
		exit(1);
	}
	while ((buf = fgetln(grey, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else
			/* all valid lines end in \n */
			continue;
		if (strlen(buf) < 4)
			continue;

		switch (state) {
		case 0:
			if (strncmp(buf, "IP:", 3) != 0)
				break;
			strlcpy(ip, buf+3, sizeof(ip));
			if (getaddrinfo(ip, NULL, &hints, &res) == 0) {
				freeaddrinfo(res);
				state = 1;
			} else
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
		if (greyscan(PATH_SPAMD_DB) == -1)
			syslog_r(LOG_NOTICE, &sdata, "scan of %s failed",
			    PATH_SPAMD_DB);
		sleep(DB_SCAN_INTERVAL);
	}
	/* NOTREACHED */
}

static void
drop_privs(void)
{
	/*
	 * lose root, continue as non-root user
	 */
	if (pw) {
		setgroups(1, &pw->pw_gid);
		setegid(pw->pw_gid);
		setgid(pw->pw_gid);
		seteuid(pw->pw_uid);
		setuid(pw->pw_uid);
	}
}

static void
convert_spamd_db(void)
{
	char 		sfn[] = "/var/db/spamd.XXXXXXXXX";
	int		r, fd = -1;
	DB 		*db1, *db2;
	BTREEINFO	btreeinfo;
	HASHINFO	hashinfo;
	DBT		dbk, dbd;

	/* try to open the db as a BTREE */
	memset(&btreeinfo, 0, sizeof(btreeinfo));
	db1 = dbopen(PATH_SPAMD_DB, O_EXLOCK|O_RDWR, 0600, DB_BTREE,
	    &btreeinfo);
	if (db1 == NULL) {
		syslog_r(LOG_ERR, &sdata,
		    "corrupt db in %s, remove and restart", PATH_SPAMD_DB);
		exit(1);
	}
	
	if ((fd = mkstemp(sfn)) == -1) {
		syslog_r(LOG_ERR, &sdata,
		    "can't convert %s: mkstemp failed (%m)", PATH_SPAMD_DB);
		exit(1);
		
	}	
	memset(&hashinfo, 0, sizeof(hashinfo));
	db2 = dbopen(sfn, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);
	if (db2 == NULL) {
		unlink(sfn);
		syslog_r(LOG_ERR, &sdata,
		    "can't convert %s:  can't dbopen %s (%m)", PATH_SPAMD_DB,
		sfn);
	}		

	memset(&dbk, 0, sizeof(dbk));
	memset(&dbd, 0, sizeof(dbd));
		for (r = db1->seq(db1, &dbk, &dbd, R_FIRST); !r;
		    r = db1->seq(db1, &dbk, &dbd, R_NEXT)) {
			if (db2->put(db2, &dbk, &dbd, 0)) {
				db2->sync(db2, 0);
				db2->close(db2);
				db1->close(db1);
				unlink(sfn);
				syslog_r(LOG_ERR, &sdata,
				    "can't convert %s - remove and restart",
				    PATH_SPAMD_DB);
				exit(1);
			}
		}
	db2->sync(db2, 0);
	db2->close(db2);
	db1->sync(db1, 0);
	db1->close(db1);
	rename(sfn, PATH_SPAMD_DB);
	close(fd);
	/* if we are dropping privs, chown to that user */
	if (pw && (chown(PATH_SPAMD_DB, pw->pw_uid, pw->pw_gid) == -1)) {
		syslog_r(LOG_ERR, &sdata,
		    "chown %s failed (%m)", PATH_SPAMD_DB);
		exit(1);
	}
}

static void
check_spamd_db(void)
{
	HASHINFO hashinfo;
	int i = -1;
	DB *db;

	/* check to see if /var/db/spamd exists, if not, create it */
	memset(&hashinfo, 0, sizeof(hashinfo));
	db = dbopen(PATH_SPAMD_DB, O_EXLOCK|O_RDWR, 0600, DB_HASH, &hashinfo);

        if (db == NULL) {
		switch (errno) {
		case ENOENT: 
			i = open(PATH_SPAMD_DB, O_RDWR|O_CREAT, 0644);
			if (i == -1) {
				syslog_r(LOG_ERR, &sdata,
				    "create %s failed (%m)", PATH_SPAMD_DB);
				exit(1);
			}
			/* if we are dropping privs, chown to that user */
			if (pw && (fchown(i, pw->pw_uid, pw->pw_gid) == -1)) {
				syslog_r(LOG_ERR, &sdata,
				    "chown %s failed (%m)", PATH_SPAMD_DB);
				exit(1);
			}
	                close(i);
			drop_privs();
			return;
			break;
		case EFTYPE:
			/*
			 *  db may be old BTREE instead of HASH, attempt to
			 *  convert.
			 */
			convert_spamd_db();
			drop_privs();
			return;
			break;
		default:
			syslog_r(LOG_ERR, &sdata, "open of %s failed (%m)",
			    PATH_SPAMD_DB);
			exit(1);
		}
	}
	db->sync(db, 0);
	db->close(db);
	drop_privs();
}


int
greywatcher(void)
{
	struct sigaction sa;

	pfdev = open("/dev/pf", O_RDWR);
	if (pfdev == -1) {
		syslog_r(LOG_ERR, &sdata, "open of /dev/pf failed (%m)");
		exit(1);
	}

	check_spamd_db();

	db_pid = fork();
	switch(db_pid) {
	case -1:
		syslog_r(LOG_ERR, &sdata, "fork failed (%m)");
		exit(1);
	case 0:
		/*
		 * child, talks to jailed spamd over greypipe,
		 * updates db. has no access to pf.
		 */
		close(pfdev);
		setproctitle("(%s update)", PATH_SPAMD_DB);
		greyreader();
		/* NOTREACHED */
		_exit(1);
	}

	/*
	 * parent, scans db periodically for changes and updates
	 * pf whitelist table accordingly.
	 */
	fclose(grey);
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = sig_term_chld;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP,  &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	setproctitle("(pf <spamd-white> update)");
	greyscanner();
	/* NOTREACHED */
	exit(1);
}

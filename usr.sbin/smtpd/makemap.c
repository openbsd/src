/*	$OpenBSD: makemap.c,v 1.50 2014/10/25 18:48:30 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@poolp.org>
 * Copyright (c) 2008-2009 Jacek Masiulaniec <jacekm@dobremiasto.net>
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
#include <sys/stat.h>
#include <sys/tree.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"
#include "log.h"

#define	PATH_ALIASES	"/etc/mail/aliases"

extern char *__progname;

__dead void	usage(void);
static int parse_map(char *);
static int parse_entry(char *, size_t, size_t);
static int parse_mapentry(char *, size_t, size_t);
static int parse_setentry(char *, size_t, size_t);
static int make_plain(DBT *, char *);
static int make_aliases(DBT *, char *);
static char *conf_aliases(char *);

DB	*db;
char	*source;
char	*oflag;
int	 dbputs;

struct smtpd	smtpd;
struct smtpd	*env = &smtpd;

enum program {
	P_MAKEMAP,
	P_NEWALIASES
} mode;

enum output_type {
	T_PLAIN,
	T_ALIASES,
	T_SET
} type;

/*
 * Stub functions so that makemap compiles using minimum object files.
 */
void
purge_config(uint8_t what)
{
	memset(env, 0, sizeof(struct smtpd));
}

int
fork_proc_backend(const char *backend, const char *conf, const char *procname)
{
	return (-1);
}

int
main(int argc, char *argv[])
{
	struct stat	 sb;
	char		 dbname[SMTPD_MAXPATHLEN];
	char		*opts;
	char		*conf;
	int		 ch;
	DBTYPE		 dbtype = DB_HASH;
	char		*p;
	mode_t		 omode;

	log_init(1);

	mode = strcmp(__progname, "newaliases") ? P_MAKEMAP : P_NEWALIASES;
	conf = CONF_FILE;
	type = T_PLAIN;
	opts = "ho:t:d:";
	if (mode == P_NEWALIASES)
		opts = "f:h";

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'd':
			if (strcmp(optarg, "hash") == 0)
				dbtype = DB_HASH;
			else if (strcmp(optarg, "btree") == 0)
				dbtype = DB_BTREE;
			else if (strcmp(optarg, "dbm") == 0)
				dbtype = DB_RECNO;
			else
				errx(1, "unsupported DB type '%s'", optarg);
			break;
		case 'f':
			conf = optarg;
			break;
		case 'o':
			oflag = optarg;
			break;
		case 't':
			if (strcmp(optarg, "aliases") == 0)
				type = T_ALIASES;
			else if (strcmp(optarg, "set") == 0)
				type = T_SET;
			else
				errx(1, "unsupported type '%s'", optarg);
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* sendmail-compat makemap ... re-execute using proper interface */
	if (argc == 2) {
		if (oflag)
			usage();

		p = strstr(argv[1], ".db");
		if (p == NULL || strcmp(p, ".db") != 0) {
			if (! bsnprintf(dbname, sizeof dbname, "%s.db",
				argv[1]))
				errx(1, "database name too long");
		}
		else {
			if (strlcpy(dbname, argv[1], sizeof dbname)
			    >= sizeof dbname)
				errx(1, "database name too long");
		}

		execlp("makemap", "makemap", "-d", argv[0], "-o", dbname, "-",
		    NULL);
		err(1, "execlp");
	}

	if (mode == P_NEWALIASES) {
		if (geteuid())
			errx(1, "need root privileges");
		if (argc != 0)
			usage();
		type = T_ALIASES;
		source = conf_aliases(conf);
	} else {
		if (argc != 1)
			usage();
		source = argv[0];
	}

	if (oflag == NULL && asprintf(&oflag, "%s.db", source) == -1)
		err(1, "asprintf");

	if (strcmp(source, "-") != 0)
		if (stat(source, &sb) == -1)
			err(1, "stat: %s", source);

	if (! bsnprintf(dbname, sizeof(dbname), "%s.XXXXXXXXXXX", oflag))
		errx(1, "path too long");
	omode = umask(7077);
	if (mkstemp(dbname) == -1)
		err(1, "mkstemp");
	umask(omode);

	db = dbopen(dbname, O_EXLOCK|O_RDWR|O_SYNC, 0644, dbtype, NULL);
	if (db == NULL) {
		warn("dbopen: %s", dbname);
		goto bad;
	}

	if (strcmp(source, "-") != 0)
		if (fchmod(db->fd(db), sb.st_mode) == -1 ||
		    fchown(db->fd(db), sb.st_uid, sb.st_gid) == -1) {
			warn("couldn't carry ownership and perms to %s",
			    dbname);
			goto bad;
		}

	if (! parse_map(source))
		goto bad;

	if (db->close(db) == -1) {
		warn("dbclose: %s", dbname);
		goto bad;
	}

	if (rename(dbname, oflag) == -1) {
		warn("rename");
		goto bad;
	}

	if (mode == P_NEWALIASES)
		printf("%s: %d aliases\n", source, dbputs);
	else if (dbputs == 0)
		warnx("warning: empty map created: %s", oflag);

	return 0;
bad:
	unlink(dbname);
	return 1;
}

int
parse_map(char *filename)
{
	FILE	*fp;
	char	*line;
	size_t	 len;
	size_t	 lineno = 0;
	char	 delim[] = { '\\', '\\', '#' };

	if (strcmp(filename, "-") == 0)
		fp = fdopen(0, "r");
	else
		fp = fopen(filename, "r");
	if (fp == NULL) {
		warn("%s", filename);
		return 0;
	}

	if (!isatty(fileno(fp)) && flock(fileno(fp), LOCK_SH|LOCK_NB) == -1) {
		if (errno == EWOULDBLOCK)
			warnx("%s is locked", filename);
		else
			warn("%s: flock", filename);
		fclose(fp);
		return 0;
	}

	while ((line = fparseln(fp, &len, &lineno, delim, 0)) != NULL) {
		if (! parse_entry(line, len, lineno)) {
			free(line);
			fclose(fp);
			return 0;
		}
		free(line);
	}

	fclose(fp);
	return 1;
}

int
parse_entry(char *line, size_t len, size_t lineno)
{
	switch (type) {
	case T_PLAIN:
	case T_ALIASES:
		return parse_mapentry(line, len, lineno);
	case T_SET:
		return parse_setentry(line, len, lineno);
	}
	return 0;
}

int
parse_mapentry(char *line, size_t len, size_t lineno)
{
	DBT	 key;
	DBT	 val;
	char	*keyp;
	char	*valp;

	keyp = line;
	while (isspace((unsigned char)*keyp))
		keyp++;
	if (*keyp == '\0' || *keyp == '#')
		return 1;

	valp = keyp;
	strsep(&valp, " \t:");
	if (valp == NULL || valp == keyp)
		goto bad;
	while (*valp == ':' || isspace((unsigned char)*valp))
		valp++;
	if (*valp == '\0' || *valp == '#')
		goto bad;

	/* Check for dups. */
	key.data = keyp;
	key.size = strlen(keyp) + 1;

	xlowercase(key.data, key.data, strlen(key.data) + 1);
	if (db->get(db, &key, &val, 0) == 0) {
		warnx("%s:%zd: duplicate entry for %s", source, lineno, keyp);
		return 0;
	}

	if (type == T_PLAIN) {
		if (! make_plain(&val, valp))
			goto bad;
	}
	else if (type == T_ALIASES) {
		if (! make_aliases(&val, valp))
			goto bad;
	}

	if (db->put(db, &key, &val, 0) == -1) {
		warn("dbput");
		return 0;
	}

	dbputs++;

	free(val.data);

	return 1;

bad:
	warnx("%s:%zd: invalid entry", source, lineno);
	return 0;
}

int
parse_setentry(char *line, size_t len, size_t lineno)
{
	DBT	 key;
	DBT	 val;
	char	*keyp;

	keyp = line;
	while (isspace((unsigned char)*keyp))
		keyp++;
	if (*keyp == '\0' || *keyp == '#')
		return 1;

	val.data  = "<set>";
	val.size = strlen(val.data) + 1;

	/* Check for dups. */
	key.data = keyp;
	key.size = strlen(keyp) + 1;
	xlowercase(key.data, key.data, strlen(key.data) + 1);
	if (db->get(db, &key, &val, 0) == 0) {
		warnx("%s:%zd: duplicate entry for %s", source, lineno, keyp);
		return 0;
	}

	if (db->put(db, &key, &val, 0) == -1) {
		warn("dbput");
		return 0;
	}

	dbputs++;

	return 1;
}

int
make_plain(DBT *val, char *text)
{
	val->data = xstrdup(text, "make_plain");
	val->size = strlen(text) + 1;

	return (val->size);
}

int
make_aliases(DBT *val, char *text)
{
	struct expandnode	xn;
	char		       *subrcpt;
	char		       *endp;
	char		       *origtext;

	val->data = NULL;
	val->size = 0;

	origtext = xstrdup(text, "make_aliases");

	while ((subrcpt = strsep(&text, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((unsigned char)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((unsigned char)*endp))
			*endp-- = '\0';

		if (! text_to_expandnode(&xn, subrcpt))
			goto error;
	}

	val->data = origtext;
	val->size = strlen(origtext) + 1;
	return (val->size);

error:
	free(origtext);

	return 0;
}

char *
conf_aliases(char *cfgpath)
{
	struct table	*table;
	char		*path;
	char		*p;

	if (parse_config(env, cfgpath, 0))
		exit(1);

	table = table_find("aliases", NULL);
	if (table == NULL)
		return (PATH_ALIASES);

	path = xstrdup(table->t_config, "conf_aliases");
	p = strstr(path, ".db");
	if (p == NULL || strcmp(p, ".db") != 0) {
		return (path);
	}
	*p = '\0';
	return (path);
}

void
usage(void)
{
	if (mode == P_NEWALIASES)
		fprintf(stderr, "usage: %s [-f file]\n", __progname);
	else
		fprintf(stderr, "usage: %s [-d dbtype] [-o dbfile] "
		    "[-t type] file\n", __progname);
	exit(1);
}

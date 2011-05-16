/*	$OpenBSD: makemap.c,v 1.32 2011/05/16 21:27:38 jasper Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/param.h>
#include <sys/socket.h>

#include <db.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <unistd.h>

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

struct smtpd	*env = NULL;

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
purge_config(u_int8_t what)
{
	bzero(env, sizeof(struct smtpd));
}

int
ssl_load_certfile(const char *name, u_int8_t flags)
{
	return (0);
}

int
main(int argc, char *argv[])
{
	struct stat	 sb;
	char		 dbname[MAXPATHLEN];
	char		*opts;
	char		*conf;
	int		 ch;
	struct smtpd	 smtpd;

	env = &smtpd;

	log_init(1);

	mode = strcmp(__progname, "newaliases") ? P_MAKEMAP : P_NEWALIASES;
	conf = CONF_FILE;
	type = T_PLAIN;
	opts = "ho:t:";
	if (mode == P_NEWALIASES)
		opts = "f:h";

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
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

	if (stat(source, &sb) == -1)
		err(1, "stat: %s", source);

	if (! bsnprintf(dbname, sizeof(dbname), "%s.XXXXXXXXXXX", oflag))
		errx(1, "path too long");
	if (mkstemp(dbname) == -1)
		err(1, "mkstemp");

	db = dbopen(dbname, O_EXLOCK|O_RDWR|O_SYNC, 0644, DB_HASH, NULL);
	if (db == NULL) {
		warn("dbopen: %s", dbname);
		goto bad;
	}

	if (fchmod(db->fd(db), sb.st_mode) == -1 ||
	    fchown(db->fd(db), sb.st_uid, sb.st_gid) == -1) {
		warn("couldn't carry ownership and perms to %s", dbname);
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
	char	 delim[] = { '\\', 0, 0 };

	fp = fopen(filename, "r");
	if (fp == NULL) {
		warn("%s", filename);
		return 0;
	}

	if (flock(fileno(fp), LOCK_SH|LOCK_NB) == -1) {
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
	while (isspace((int)*keyp))
		keyp++;
	if (*keyp == '\0' || *keyp == '#')
		return 1;

	valp = keyp;
	strsep(&valp, " \t:");
	if (valp == NULL || valp == keyp)
		goto bad;
	while (*valp == ':' || isspace((int)*valp))
		valp++;
	if (*valp == '\0' || *valp == '#')
		goto bad;

	/* Check for dups. */
	key.data = keyp;
	key.size = strlen(keyp) + 1;
	if (db->get(db, &key, &val, 0) == 0) {
		warnx("%s:%zd: duplicate entry for %s", source, lineno, keyp);
		return 0;
	}

	if (type == T_PLAIN) {
		if (! make_plain(&val, valp))
			goto bad;
	}
	else if (type == T_ALIASES) {
		lowercase(key.data, key.data, strlen(key.data) + 1);
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
	while (isspace((int)*keyp))
		keyp++;
	if (*keyp == '\0' || *keyp == '#')
		return 1;

	val.data  = "<set>";
	val.size = strlen(val.data) + 1;

	/* Check for dups. */
	key.data = keyp;
	key.size = strlen(keyp) + 1;
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
	val->data = strdup(text);
	if (val->data == NULL)
		err(1, "malloc");

	val->size = strlen(text) + 1;

	return (val->size);
}

int
make_aliases(DBT *val, char *text)
{
	struct expandnode	expnode;
	char	       	*subrcpt;
	char	       	*endp;
	char		*origtext;

	val->data = NULL;
	val->size = 0;

	origtext = strdup(text);
	if (origtext == NULL)
		fatal("strdup");

	while ((subrcpt = strsep(&text, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((int)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((int)*endp))
			*endp-- = '\0';

		bzero(&expnode, sizeof(struct expandnode));
		if (! alias_parse(&expnode, subrcpt))
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
	struct map	*map;
	char		*path;
	char		*p;

	if (parse_config(env, cfgpath, 0))
		exit(1);

	map = map_findbyname("aliases");
	if (map == NULL)
		return (PATH_ALIASES);

	path = strdup(map->m_config);
	if (path == NULL)
		err(1, NULL);
	p = strstr(path, ".db");
	if (p == NULL || p[3] != '\0')
		errx(1, "%s: %s: no .db suffix present", cfgpath, path);
	*p = '\0';

	return (path);
}

void
usage(void)
{
	if (mode == P_NEWALIASES)
		fprintf(stderr, "usage: %s [-f file]\n", __progname);
	else
		fprintf(stderr, "usage: %s [-o dbfile] [-t type] file\n",
		    __progname);
	exit(1);
}

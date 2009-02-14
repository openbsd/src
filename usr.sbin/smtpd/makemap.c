/*	$OpenBSD: makemap.c,v 1.8 2009/02/14 18:37:12 jacekm Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
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

#include <ctype.h>
#include <db.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "smtpd.h"

extern char *__progname;

__dead void	usage(void);
int		parse_map(char *);
int		parse_entry(char *, size_t, size_t);
int		make_plain(DBT *, char *);
int		make_aliases(DBT *, char *);

DB	*db;
char	*source;
char	*oflag;
int	 dbputs;

enum program {
	P_MAKEMAP,
	P_NEWALIASES
} mode;

enum output_type {
	T_PLAIN,
	T_ALIASES
} type;

int
main(int argc, char *argv[])
{
	char	 dbname[MAXPATHLEN];
	char	*opts;
	int	 ch;

	mode = strcmp(__progname, "newaliases") ? P_MAKEMAP : P_NEWALIASES;
	type = T_PLAIN;
	opts = "ho:t:";
	if (mode == P_NEWALIASES)
		opts = "h";

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'o':
			oflag = optarg;
			break;
		case 't':
			if (strcmp(optarg, "aliases") == 0)
				type = T_ALIASES;
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
		source = PATH_ALIASES;
	} else {
		if (argc != 1)
			usage();
		source = argv[0];
	}

	if (oflag == NULL && asprintf(&oflag, "%s.db", source) == -1)
		err(1, "asprintf");

	if (! bsnprintf(dbname, MAXPATHLEN, "%s.XXXXXXXXXXX", oflag))
		errx(1, "path too long");
	if (mkstemp(dbname) == -1)
		err(1, "mkstemp");

	db = dbopen(dbname, O_EXLOCK|O_RDWR|O_SYNC, 0644, DB_HASH, NULL);
	if (db == NULL) {
		warn("dbopen: %s", dbname);
		goto bad;
	}

	if (! parse_map(source))
		goto bad;

	if (db->close(db) == -1) {
		warn("dbclose: %s", dbname);
		goto bad;
	}

	if (chmod(dbname, 0644) == -1) {
		warn("chmod: %s", dbname);
		goto bad;
	}

	if (rename(dbname, oflag) == -1) {
		warn("rename");
		goto bad;
	}

	if (dbputs == 0)
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

	fp = fopen(filename, "r");
	if (fp == NULL) {
		warn("%s", filename);
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
	DBT	 key;
	DBT	 val;
	char	*keyp;
	char	*valp;

	keyp = line;
	while (isspace(*keyp))
		keyp++;
	if (*keyp == '\0')
		return 1;

	valp = keyp;
	strsep(&valp, " \t:");
	if (valp == NULL || valp == keyp)
		goto bad;

	/* Check for dups. */
	key.data = keyp;
	key.size = strlen(keyp) + 1;
	if (db->get(db, &key, &val, 0) == 0) {
		warnx("%s:%zd: duplicate entry for %s", source, lineno, keyp);
		return 0;
	}

	switch (type) {
	case T_PLAIN:
		if (! make_plain(&val, valp))
			goto bad;
		break;
	case T_ALIASES:
		if (! make_aliases(&val, valp))
			goto bad;
		break;
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
make_plain(DBT *val, char *text)
{
	struct alias	*a;

	a = calloc(1, sizeof(struct alias));
	if (a == NULL)
		err(1, "calloc");

	a->type = ALIAS_TEXT;
	val->data = a;
	val->size = strlcpy(a->u.text, text, sizeof(a->u.text));

	if (val->size >= sizeof(a->u.text)) {
		free(a);
		return 0;
	}

	return (val->size);
}

int
make_aliases(DBT *val, char *text)
{
	struct alias	 a;
	char		*subrcpt;
	char		*endp;

	val->data = NULL;
	val->size = 0;

	while ((subrcpt = strsep(&text, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace(*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace(*endp))
			*endp-- = '\0';

		if (! alias_parse(&a, subrcpt))
			goto error;

		val->data = realloc(val->data, val->size + sizeof(a));
		if (val->data == NULL)
			err(1, "get_targets: realloc");
		memcpy((u_int8_t *)val->data + val->size, &a, sizeof(a));
		val->size += sizeof(a);
	}

	return (val->size);

error:
	free(val->data);

	return 0;
}

void
usage(void)
{
	if (mode == P_NEWALIASES)
		fprintf(stderr, "usage: %s\n", __progname);
	else
		fprintf(stderr, "usage: %s [-t type] [-o dbfile] file\n",
		    __progname);
	exit(1);
}

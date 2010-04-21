/*	$OpenBSD: map.c,v 1.11 2010/04/21 19:45:07 gilles Exp $	*/

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
#include <sys/queue.h>
#include <sys/tree.h>
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

#include "smtpd.h"

struct map_backend *map_backend_lookup(enum map_src);
struct map_parser *map_parser_lookup(enum map_kind);

/* db(3) backend */
void *map_db_open(char *);
void map_db_close(void *);
char *map_db_get(void *, char *);
int map_db_put(void *, char *, char *);

/* stdio(3) backend */
void *map_stdio_open(char *);
void map_stdio_close(void *);
char *map_stdio_get(void *, char *);
int map_stdio_put(void *, char *, char *);


struct map_backend {
	enum map_src source;
	void *(*open)(char *);
	void (*close)(void *);
	char *(*get)(void *, char *);
	int (*put)(void *, char *, char *);
} map_backends[] = {
	{ S_DB,
	  map_db_open, map_db_close, map_db_get, map_db_put },
	{ S_FILE,
	  map_stdio_open, map_stdio_close, map_stdio_get, map_stdio_put },
};

struct map_parser {
	enum map_kind kind;
	void *(*extract)(char *, size_t len);
} map_parsers[] = {
	{ K_NONE, NULL },
	{ K_ALIASES, NULL },
	{ K_CREDENTIALS, NULL }
};

struct map *
map_findbyname(struct smtpd *env, const char *name)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (strcmp(m->m_name, name) == 0)
			break;
	}
	return (m);
}

struct map *
map_find(struct smtpd *env, objid_t id)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (m->m_id == id)
			break;
	}
	return (m);
}

char *
map_lookup(struct smtpd *env, objid_t mapid, char *key)
{
	void *hdl = NULL;
	char *result = NULL;
	struct map *map;
	struct map_backend *backend = NULL;

	map = map_find(env, mapid);
	if (map == NULL)
		return NULL;

	backend = map_backend_lookup(map->m_src);

	hdl = backend->open(map->m_config);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return NULL;
	}

	result = backend->get(hdl, key);
	backend->close(hdl);

	return result;
}

struct map_backend *
map_backend_lookup(enum map_src source)
{
	u_int8_t i;

	for (i = 0; i < nitems(map_backends); ++i)
		if (map_backends[i].source == source)
			break;

	if (i == nitems(map_backends))
		fatalx("invalid map type");

	return &map_backends[i];
}

struct map_parser *
map_parser_lookup(enum map_kind kind)
{
	u_int8_t i;

	for (i = 0; i < nitems(map_parsers); ++i)
		if (map_parsers[i].kind == kind)
			break;

	if (i == nitems(map_parsers))
		fatalx("invalid map kind");

	return &map_parsers[i];
}

/* db(3) backend */
void *
map_db_open(char *src)
{
	return dbopen(src, O_RDONLY, 0600, DB_HASH, NULL);
}

void
map_db_close(void *hdl)
{
	DB *db = hdl;

	db->close(db);
}

char *
map_db_get(void *hdl, char *key)
{
	int ret;
	DBT dbk;
	DBT dbv;
	DB *db = hdl;
	char *result = NULL;

	dbk.data = key;
	dbk.size = strlen(dbk.data) + 1;

	if ((ret = db->get(db, &dbk, &dbv, 0)) != 0)
		return NULL;

	result = calloc(dbv.size, 1);
	if (result == NULL)
		fatal("calloc");
	(void)strlcpy(result, dbv.data, dbv.size);

	return result;
}

int
map_db_put(void *hdl, char *key, char *val)
{
	return 0;
}


/* stdio(3) backend */
void *
map_stdio_open(char *src)
{
	return fopen(src, "r");
}

void
map_stdio_close(void *hdl)
{
	FILE *fp = hdl;

	fclose(fp);
}

char *
map_stdio_get(void *hdl, char *key)
{
	char *buf, *lbuf;
	size_t len;
	char *keyp;
	char *valp;
	FILE *fp = hdl;
	char *result = NULL;

	lbuf = NULL;
	while ((buf = fgetln(fp, &len))) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
		else {
			if ((lbuf = malloc(len + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, len);
			lbuf[len] = '\0';
			buf = lbuf;
		}

		keyp = buf;
		while (isspace((int)*keyp))
			++keyp;
		if (*keyp == '\0' || *keyp == '#')
			continue;

		valp = keyp;
		strsep(&valp, " \t:");
		if (valp == NULL || valp == keyp)
			continue;

		if (strcmp(keyp, key) != 0)
			continue;

		result = strdup(buf);
		if (result == NULL)
			err(1, NULL);
		break;
	}
	free(lbuf);

	return result;
}

int
map_stdio_put(void *hdl, char *key, char *val)
{
	return 0;
}

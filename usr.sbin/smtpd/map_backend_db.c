/*	$OpenBSD: map_backend_db.c,v 1.1 2011/05/21 18:43:08 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <db.h>
#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"


/* db(3) backend */
static void *map_db_open(char *);
static void *map_db_lookup(void *, char *, enum map_kind);
static void  map_db_close(void *);

static char *map_db_get_entry(void *, char *, size_t *);
static void *map_db_secret(char *, char *, size_t);
static void *map_db_alias(char *, char *, size_t);
static void *map_db_virtual(char *, char *, size_t);


struct map_backend map_backend_db = {
	map_db_open,
	map_db_close,
	map_db_lookup
};


static void *
map_db_open(char *src)
{
	return dbopen(src, O_RDONLY, 0600, DB_HASH, NULL);
}

static void
map_db_close(void *hdl)
{
	DB *db = hdl;

	db->close(db);
}

static void *
map_db_lookup(void *hdl, char *key, enum map_kind kind)
{
	char *line;
	size_t len;
	void *ret;

	line = map_db_get_entry(hdl, key, &len);
	if (line == NULL)
		return NULL;

	ret = 0;
	switch (kind) {
	case K_ALIAS:
		ret = map_db_alias(key, line, len);
		break;

	case K_SECRET:
		ret = map_db_secret(key, line, len);
		break;

	case K_VIRTUAL:
		ret = map_db_virtual(key, line, len);
		break;

	default:
		break;
	}

	free(line);

	return ret;
}


static char *
map_db_get_entry(void *hdl, char *key, size_t *len)
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

	*len = dbv.size;

	return result;
}

static void *
map_db_secret(char *key, char *line, size_t len)
{
	struct map_secret *map_secret = NULL;
	char *p;

	/* credentials are stored as user:password */
	if (len < 3)
		return NULL;

	/* too big to fit in a smtp session line */
	if (len >= MAX_LINE_SIZE)
		return NULL;

	p = strchr(line, ':');
	if (p == NULL)
		return NULL;

	if (p == line || p == line + len - 1)
		return NULL;
	*p++ = '\0';

	map_secret = calloc(1, sizeof(struct map_secret));
	if (map_secret == NULL)
		fatalx("calloc");

	if (strlcpy(map_secret->username, line,
		sizeof(map_secret->username)) >=
	    sizeof(map_secret->username))
		goto err;

	if (strlcpy(map_secret->password, p,
		sizeof(map_secret->password)) >=
	    sizeof(map_secret->password))
		goto err;

	return map_secret;

err:
	free(map_secret);
	return NULL;
}

static void *
map_db_alias(char *key, char *line, size_t len)
{
	char	       	*subrcpt;
	char	       	*endp;
	struct map_alias	*map_alias = NULL;
	struct expandnode	 expnode;

	map_alias = calloc(1, sizeof(struct map_alias));
	if (map_alias == NULL)
		fatalx("calloc");

	while ((subrcpt = strsep(&line, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((int)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((int)*endp))
			*endp-- = '\0';

		bzero(&expnode, sizeof (struct expandnode));
		if (! alias_parse(&expnode, subrcpt))
			goto error;

		expandtree_increment_node(&map_alias->expandtree, &expnode);
		map_alias->nbnodes++;
	}

	return map_alias;

error:
	/* free elements in map_alias->expandtree */
	expandtree_free_nodes(&map_alias->expandtree);
	free(map_alias);
	return NULL;
}

static void *
map_db_virtual(char *key, char *line, size_t len)
{
	char	       	*subrcpt;
	char	       	*endp;
	struct map_virtual	*map_virtual = NULL;
	struct expandnode	 expnode;

	map_virtual = calloc(1, sizeof(struct map_virtual));
	if (map_virtual == NULL)
		fatalx("calloc");

	/* domain key, discard value */
	if (strchr(key, '@') == NULL)
		return map_virtual;

	while ((subrcpt = strsep(&line, ",")) != NULL) {
		/* subrcpt: strip initial whitespace. */
		while (isspace((int)*subrcpt))
			++subrcpt;
		if (*subrcpt == '\0')
			goto error;

		/* subrcpt: strip trailing whitespace. */
		endp = subrcpt + strlen(subrcpt) - 1;
		while (subrcpt < endp && isspace((int)*endp))
			*endp-- = '\0';

		bzero(&expnode, sizeof (struct expandnode));
		if (! alias_parse(&expnode, subrcpt))
			goto error;

		expandtree_increment_node(&map_virtual->expandtree, &expnode);
		map_virtual->nbnodes++;
	}

	return map_virtual;

error:
	/* free elements in map_virtual->expandtree */
	expandtree_free_nodes(&map_virtual->expandtree);
	free(map_virtual);
	return NULL;
}

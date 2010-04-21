/*	$OpenBSD: map_backend.c,v 1.1 2010/04/21 21:04:29 gilles Exp $	*/

/*
 * Copyright (c) 2010 Gilles Chehade <gilles@openbsd.org>
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

/* db(3) backend */
void *map_db_open(char *);
void map_db_close(void *);
char *map_db_get(void *, char *, size_t *);
int map_db_put(void *, char *, char *);

/* stdio(3) backend */
void *map_stdio_open(char *);
void map_stdio_close(void *);
char *map_stdio_get(void *, char *, size_t *);
int map_stdio_put(void *, char *, char *);


struct map_backend map_backends[] = {
	{ S_DB,
	  map_db_open, map_db_close, map_db_get, map_db_put },
	{ S_FILE,
	  map_stdio_open, map_stdio_close, map_stdio_get, map_stdio_put },
};


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
map_db_get(void *hdl, char *key, size_t *len)
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
map_stdio_get(void *hdl, char *key, size_t *len)
{
	char *buf, *lbuf;
	size_t flen;
	char *keyp;
	char *valp;
	FILE *fp = hdl;
	char *result = NULL;

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			if ((lbuf = malloc(flen + 1)) == NULL)
				err(1, NULL);
			memcpy(lbuf, buf, flen);
			lbuf[flen] = '\0';
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

		result = strdup(valp);
		if (result == NULL)
			err(1, NULL);
		*len = strlen(result);

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

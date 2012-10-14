/*	$OpenBSD: map_file.c,v 1.1 2012/10/14 11:58:23 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

/* file backend */
static void *map_file_open(struct map *);
static void  map_file_update(struct map *);
static void *map_file_lookup(void *, const char *, enum map_kind);
static int   map_file_compare(void *, const char *, enum map_kind,
    int (*)(const char *, const char *));
static void  map_file_close(void *);

struct map_backend *map_backend_lookup(enum map_src);

struct map_backend map_backend_file = {
	map_file_open,
	map_file_update,
	map_file_close,
	map_file_lookup,
	map_file_compare
};

static void
file_load(struct map *m, FILE *fp)
{
	char *buf, *lbuf;
	size_t flen;
	char *keyp;
	char *valp;

	lbuf = NULL;
	while ((buf = fgetln(fp, &flen))) {
		if (buf[flen - 1] == '\n')
			buf[flen - 1] = '\0';
		else {
			lbuf = xmalloc(flen + 1, "map_stdio_get_entry");
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
		if (valp) {
			while (*valp && isspace(*valp))
				++valp;
			if (*valp == '\0')
				valp = NULL;
		}
		map_add(m, keyp, valp == keyp ? NULL : valp);
	}
	free(lbuf);
}


static void *
map_file_open(struct map *map)
{
	FILE		*fp = NULL;
	struct map	*mp = NULL;

	if (map->m_handle)
		return map->m_handle;

	mp = map_create(S_NONE, NULL);

	fp = fopen(map->m_config, "r");
	if (fp == NULL)
		goto err;
	file_load(mp, fp);
	fclose(fp);

	map->m_handle = mp;

	log_info("map_file_open: initialized map \"%s\" from %s",
	    map->m_name, map->m_config);

	return mp;

err:
	if (mp)
		map_destroy(mp);

	if (fp)
		fclose(fp);

	return NULL;	
}

static void
map_file_update(struct map *map)
{
	FILE		*fp = NULL;
	struct map	*mp = NULL;

	fp = fopen(map->m_config, "r");
	if (fp == NULL) {
		log_info("map_file_update: could not update map \"%s\" from %s: %s",
		    map->m_name, map->m_config, strerror(errno));
		return;
	}

	mp = map_create(S_NONE, NULL);
	file_load(mp, fp);
	fclose(fp);

	if (map->m_handle != NULL) {
		map_destroy(map->m_handle);
		map->m_handle = NULL;
	}

	map->m_handle = mp;
	log_info("map_file_update: updated map \"%s\" from %s",
	    map->m_name, map->m_config);
}

static void
map_file_close(void *hdl)
{
	/* ignore */
}

static void *
map_file_lookup(void *hdl, const char *key, enum map_kind kind)
{
	struct map		*mp = hdl;

	return map_lookup(mp->m_id, key, kind);
}

static int
map_file_compare(void *hdl, const char *key, enum map_kind kind,
    int (*func)(const char *, const char *))
{
	struct map		*mp = hdl;

	return map_compare(mp->m_id, key, kind, func);
}

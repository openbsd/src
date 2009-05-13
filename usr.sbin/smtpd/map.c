/*	$OpenBSD: map.c,v 1.6 2009/05/13 21:20:55 jacekm Exp $	*/

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

#include <db.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"

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
map_dblookup(struct smtpd *env, char *mapname, char *keyname)
{
	int ret;
	DBT key;
	DBT val;
	DB *db;
	struct map *map;
	char *result = NULL;

	map = map_findbyname(env, mapname);
	if (map == NULL)
		return NULL;

	if (map->m_src != S_DB) {
		log_warn("invalid map type for map \"%s\"", mapname);
		return NULL;
	}

	db = dbopen(map->m_config, O_RDONLY, 0600, DB_HASH, NULL);
	if (db == NULL) {
		log_warn("map_dblookup: can't open %s", map->m_config);
		return NULL;
	}

	key.data = keyname;
	key.size = strlen(key.data) + 1;

	if ((ret = db->get(db, &key, &val, 0)) == -1) {
		log_warn("map_dblookup: map '%s'", mapname);
		db->close(db);
		return NULL;
	}

	if (ret == 0) {
		result = calloc(val.size, 1);
		if (result == NULL)
			fatal("calloc");
		(void)strlcpy(result, val.data, val.size);
	}

	db->close(db);

	return ret == 0 ? result : NULL;
}

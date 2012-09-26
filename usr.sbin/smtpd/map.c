/*	$OpenBSD: map.c,v 1.31 2012/09/26 12:28:08 eric Exp $	*/

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

#include <err.h>
#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

struct map_backend *map_backend_lookup(enum map_src);

extern struct map_backend map_backend_static;

extern struct map_backend map_backend_db;
extern struct map_backend map_backend_stdio;
/* extern struct map_backend map_backend_ldap; */

static objid_t	last_map_id = 0;

struct map_backend *
map_backend_lookup(enum map_src source)
{
	switch (source) {
	case S_NONE:
		return &map_backend_static;

	case S_DB:
		return &map_backend_db;

	case S_PLAIN:
		return &map_backend_stdio;
/*
	case S_LDAP:
		return &map_backend_ldap;
*/
	default:
		fatalx("invalid map type");
	}
	return NULL;
}

struct map *
map_findbyname(const char *name)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (strcmp(m->m_name, name) == 0)
			break;
	}
	return (m);
}

struct map *
map_find(objid_t id)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (m->m_id == id)
			break;
	}
	return (m);
}

void *
map_lookup(objid_t mapid, const char *key, enum map_kind kind)
{
	void *hdl = NULL;
	char *ret = NULL;
	struct map *map;
	struct map_backend *backend = NULL;

	map = map_find(mapid);
	if (map == NULL)
		return NULL;

	backend = map_backend_lookup(map->m_src);
	hdl = backend->open(map);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return NULL;
	}

	ret = backend->lookup(hdl, key, kind);

	backend->close(hdl);
	return ret;
}

int
map_compare(objid_t mapid, const char *key, enum map_kind kind,
    int (*func)(const char *, const char *))
{
	void *hdl = NULL;
	struct map *map;
	struct map_backend *backend = NULL;
	int ret;

	map = map_find(mapid);
	if (map == NULL)
		return 0;

	backend = map_backend_lookup(map->m_src);
	hdl = backend->open(map);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return 0;
	}

	ret = backend->compare(hdl, key, kind, func);

	backend->close(hdl);
	return ret;	
}

struct map*
map_create(enum map_kind kind, const char *name)
{
	struct map	*m;
	size_t		 n;

	if (name && map_findbyname(name))
		errx(1, "map_create: map \"%s\" already defined", name);

	m = xcalloc(1, sizeof(*m), "map_create");
	m->m_src = kind;
	m->m_id = ++last_map_id;
	if (m->m_id == INT_MAX)
		errx(1, "map_create: too many maps defined");

	if (name == NULL)
		snprintf(m->m_name, sizeof(m->m_name), "<dynamic:%u>", m->m_id);
	else {
		n = strlcpy(m->m_name, name, sizeof(m->m_name));
		if (n >= sizeof(m->m_name))
			errx(1, "map_create: map name too long");
	}

	TAILQ_INIT(&m->m_contents);

	TAILQ_INSERT_TAIL(env->sc_maps, m, m_entry);

	return (m);
}

void
map_add(struct map *m, const char *key, const char * val)
{
	struct mapel	*me;
	size_t		 n;

	if (m->m_src != S_NONE)
		errx(1, "map_add: cannot add to map");

	me = xcalloc(1, sizeof(*me), "map_add");
	n = strlcpy(me->me_key.med_string, key, sizeof(me->me_key.med_string));
	if (n >= sizeof(me->me_key.med_string))
		errx(1, "map_add: key too long");

	if (val) {
		n = strlcpy(me->me_val.med_string, key,
		    sizeof(me->me_val.med_string));
		if (n >= sizeof(me->me_val.med_string))
			errx(1, "map_add: value too long");
	}
	
	TAILQ_INSERT_TAIL(&m->m_contents, me, me_entry);
}

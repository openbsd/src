/*	$OpenBSD: map.c,v 1.14 2010/04/21 21:04:29 gilles Exp $	*/

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
map_lookup(struct smtpd *env, objid_t mapid, char *key, enum map_kind kind)
{
	void *hdl = NULL;
	char *result = NULL;
	char *ret = NULL;
	size_t len;
	struct map *map;
	struct map_backend *backend = NULL;
	struct map_parser *parser = NULL;

	map = map_find(env, mapid);
	if (map == NULL)
		return NULL;

	backend = map_backend_lookup(map->m_src);
	parser  = map_parser_lookup(kind);

	hdl = backend->open(map->m_config);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return NULL;
	}

	ret = result = backend->get(hdl, key, &len);
	if (ret == NULL)
		goto end;

	if (parser->extract != NULL) {
		ret = parser->extract(result, len);
		free(result);
	}

end:
	backend->close(hdl);
	return ret;
}

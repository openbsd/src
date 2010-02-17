/*	$OpenBSD: ruleset.c,v 1.9 2010/02/17 13:47:31 gilles Exp $ */

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@openbsd.org>
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

#include <netinet/in.h>
#include <arpa/inet.h>

#include <db.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"

struct rule    *ruleset_match(struct smtpd *, char *tag, struct path *, struct sockaddr_storage *);
int		ruleset_check_source(struct map *, struct sockaddr_storage *);
int		ruleset_match_mask(struct sockaddr_storage *, struct netaddr *);


struct rule *
ruleset_match(struct smtpd *env, char *tag, struct path *path, struct sockaddr_storage *ss)
{
	struct rule *r;
	struct cond *cond;
	struct map *map;
	struct mapel *me;

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {

		if (r->r_tag[0] != '\0' && strcmp(r->r_tag, tag) != 0)
			continue;

		if (ss != NULL &&
		    (!(path->flags & F_PATH_AUTHENTICATED) &&
			! ruleset_check_source(r->r_sources, ss)))
			continue;

		TAILQ_FOREACH(cond, &r->r_conditions, c_entry) {
			if (cond->c_type == C_ALL) {
				path->cond = cond;
				return r;
			}

			if (cond->c_type == C_DOM) {
				map = map_find(env, cond->c_map);
				if (map == NULL)
					fatal("failed to lookup map.");

				switch (map->m_src) {
				case S_NONE:
					TAILQ_FOREACH(me, &map->m_contents, me_entry) {
						if (hostname_match(path->domain, me->me_key.med_string)) {
							path->cond = cond;
							return r;
						}
					}
					break;
				case S_DB:
					if (map_lookup(env, map->m_id, path->domain) != NULL) {
						path->cond = cond;
						return r;
					}
					break;
				default:
					log_info("unsupported map source for domain map");
					continue;
				}
			}

			if (cond->c_type == C_VDOM) {
				if (aliases_vdomain_exists(env, cond->c_map, path->domain)) {
					path->cond = cond;
					return r;
				}
			}
		}
	}

	return NULL;
}

int
ruleset_check_source(struct map *map, struct sockaddr_storage *ss)
{
	struct mapel *me;

	if (ss == NULL) {
		/* This happens when caller is part of an internal
		 * lookup (ie: alias resolved to a remote address)
		 */
		return 1;
	}

	TAILQ_FOREACH(me, &map->m_contents, me_entry) {

		if (ss->ss_family != me->me_key.med_addr.ss.ss_family)
			continue;

		if (ss->ss_len != me->me_key.med_addr.ss.ss_len)
			continue;

		if (ruleset_match_mask(ss, &me->me_key.med_addr))
			return 1;
	}

	return 0;
}

int
ruleset_match_mask(struct sockaddr_storage *ss, struct netaddr *ssmask)
{
	if (ss->ss_family == AF_INET) {
		struct sockaddr_in *ssin = (struct sockaddr_in *)ss;
		struct sockaddr_in *ssinmask = (struct sockaddr_in *)&ssmask->ss;

		if ((ssin->sin_addr.s_addr & ssinmask->sin_addr.s_addr) ==
		    ssinmask->sin_addr.s_addr)
			return (1);
		return (0);
	}

	if (ss->ss_family == AF_INET6) {
		struct in6_addr	*in;
		struct in6_addr	*inmask;
		struct in6_addr	 mask;
		int		 i;

		bzero(&mask, sizeof(mask));
		for (i = 0; i < (128 - ssmask->bits) / 8; i++)
			mask.s6_addr[i] = 0xff;
		i = ssmask->bits % 8;
		if (i)
			mask.s6_addr[ssmask->bits / 8] = 0xff00 >> i;

		in = &((struct sockaddr_in6 *)ss)->sin6_addr;
		inmask = &((struct sockaddr_in6 *)&ssmask->ss)->sin6_addr;

		for (i = 0; i < 16; i++) {
			if ((in->s6_addr[i] & mask.s6_addr[i]) !=
			    inmask->s6_addr[i])
				return (0);
		}
		return (1);
	}

	return (0);
}

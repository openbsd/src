/*	$OpenBSD: area.c,v 1.2 2005/01/28 17:53:33 norby Exp $ */

/*
 * Copyright (c) 2004, 2005 Esben Norby <norby@openbsd.org>
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
#include <sys/socket.h>
#include <sys/tree.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <err.h>
#include <stdlib.h>

#include "ospf.h"
#include "ospfd.h"
#include "ospfe.h"
#include "rde.h"
#include "log.h"

struct area *
area_new(void)
{
	struct area *area = NULL;

	if ((area = calloc(1, sizeof(*area))) == NULL)
		errx(1, "area_new: calloc");

	LIST_INIT(&area->iface_list);
	LIST_INIT(&area->nbr_list);
	RB_INIT(&area->lsa_tree);

	return (area);
}

int
area_del(struct area *area)
{
	struct iface		*iface = NULL;
	struct vertex		*v, *nv;
	struct rde_nbr		*n;

	log_debug("area_del: area ID %s", inet_ntoa(area->id));

	/* clean lists */
	while ((iface = LIST_FIRST(&area->iface_list)) != NULL) {
		LIST_REMOVE(iface, entry);
		if_del(iface);
	}

	while ((n = LIST_FIRST(&area->nbr_list)) != NULL)
		rde_nbr_del(n);

	for (v = RB_MIN(lsa_tree, &area->lsa_tree); v != NULL; v = nv) {
		nv = RB_NEXT(lsa_tree, &area->lsa_tree, v);
		RB_REMOVE(lsa_tree, &area->lsa_tree, v);
		vertex_free(v);
	}

	free(area);

	return (0);
}

struct area *
area_find(struct ospfd_conf *conf, struct in_addr area_id)
{
	struct area *area = NULL;

	LIST_FOREACH(area, &conf->area_list, entry) {
		if (area->id.s_addr == area_id.s_addr) {
			return (area);
		}
	}

	log_debug("area_find: area ID %s not found", inet_ntoa(area_id));
	return (NULL);
}

/*	$OpenBSD: config.c,v 1.12 2003/12/30 13:03:27 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include <sys/mman.h>

#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"

void			*sconf;

u_int32_t	get_bgpid(void);
u_int32_t	get_id(struct peer *);

int
merge_config(struct bgpd_config *xconf, struct bgpd_config *conf)
{
	enum reconf_action	 reconf = RECONF_NONE;
	struct peer		*p, *next;

	/* merge conf (new) into xconf (old)  */
	if (!conf->as) {
		logit(LOG_CRIT, "configuration error: AS not given");
		return (1);
	}
	if (xconf->as != conf->as) {
		xconf->as = conf->as;
		reconf = RECONF_REINIT;
	}
	if (conf->bgpid && xconf->bgpid != conf->bgpid) {
		xconf->bgpid = conf->bgpid;
		reconf = RECONF_REINIT;
	}
	if (!xconf->bgpid)
		xconf->bgpid = get_bgpid();

	if (conf->holdtime && !xconf->holdtime)
		xconf->holdtime = conf->holdtime;
	if (!conf->holdtime && xconf->holdtime)
		conf->holdtime = xconf->holdtime;

	if (conf->min_holdtime && !xconf->min_holdtime)
		xconf->min_holdtime = conf->min_holdtime;
	if (!conf->min_holdtime && xconf->min_holdtime)
		conf->min_holdtime = xconf->min_holdtime;
	if (!xconf->min_holdtime)
		xconf->min_holdtime = conf->min_holdtime = MIN_HOLDTIME;

	memcpy(&xconf->listen_addr, &conf->listen_addr,
	    sizeof(xconf->listen_addr));

	if ((xconf->flags & BGPD_FLAG_NO_FIB_UPDATE) !=
	    (conf->flags & BGPD_FLAG_NO_FIB_UPDATE)) {
		if (!(conf->flags & BGPD_FLAG_NO_FIB_UPDATE))
			kroute_fib_couple();
		else
			kroute_fib_decouple();
	}

	xconf->flags = conf->flags;
	xconf->log = conf->log;

	/*
	 * as we cannot get the negotiated holdtime in the main process,
	 * the session engine needs to check it against the possibly new values
	 * and decide on session reestablishment.
	 */

	xconf->holdtime = conf->holdtime;
	xconf->min_holdtime = conf->min_holdtime;

	for (p = conf->peers; p != NULL; p = p->next) {
		p->conf.reconf_action = reconf;
		p->conf.ebgp = (p->conf.remote_as != xconf->as);
		if (!p->conf.id)
			p->conf.id = get_id(p);
	}

	for (p = xconf->peers; p != NULL; p = next) {
		next = p->next;
		free(p);
	}

	/* merge peers done by session engine except for initial config */
	xconf->peers = conf->peers;

	return (0);
}

u_int32_t
get_bgpid(void)
{
	struct ifaddrs		*ifap, *ifa;
	u_int32_t		 ip = 0, cur, localnet;

	localnet = inet_addr("127.0.0.0");

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
		cur = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if ((cur & localnet) == localnet)	/* skip 127/8 */
			continue;
		if (cur > ip)
			ip = cur;
	}
	freeifaddrs(ifap);

	return (ip);
}

u_int32_t
get_id(struct peer *p)
{
	/*
	 * XXX this collides with multiviews and will need more clue later XXX
	 */
	return (ntohl(p->conf.remote_addr.sin_addr.s_addr));
}

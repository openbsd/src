/*	$OpenBSD: config.c,v 1.20 2004/01/26 14:42:47 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include "session.h"

void			*sconf;

u_int32_t	get_bgpid(void);
u_int32_t	get_id(struct peer *);

int
merge_config(struct bgpd_config *xconf, struct bgpd_config *conf,
    struct peer *peer_l)
{
	struct peer		*p;

	if (!conf->as) {
		log_warnx("configuration error: AS not given");
		return (1);
	}

	if (!conf->min_holdtime)
		conf->min_holdtime = MIN_HOLDTIME;

	if (!conf->bgpid)
		conf->bgpid = get_bgpid();

	for (p = peer_l; p != NULL; p = p->next) {
		p->conf.ebgp = (p->conf.remote_as != conf->as);
		if (!p->conf.id)
			p->conf.id = get_id(p);
		if (p->conf.tcp_sign_key[0] &&
		    pfkey_signature((struct sockaddr *)&p->conf.local_addr,
			    (struct sockaddr *)&p->conf.remote_addr,
			    p->conf.tcp_sign_key) == -1)
				return (1);
	}

	memcpy(xconf, conf, sizeof(struct bgpd_config));

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

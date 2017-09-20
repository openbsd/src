/*	$OpenBSD: privsep.c,v 1.71 2017/09/20 19:21:00 krw Exp $ */

/*
 * Copyright (c) 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE, ABUSE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <imsg.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"
#include "privsep.h"

void
dispatch_imsg(char *name, int rdomain, int ioctlfd, int routefd,
    struct imsgbuf *ibuf)
{
	static char	*resolv_conf;
	static int	 lastidx;
	struct imsg	 imsg;
	ssize_t		 n;
	size_t		 sz;
	int		 index, newidx, retries;

	index = if_nametoindex(name);
	if (index == 0) {
		log_warnx("%s: unknown interface", log_procname);
		quit = INTERNALSIG;
		return;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_DELETE_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_delete_address))
				log_warnx("%s: bad IMSG_DELETE_ADDRESS",
				    log_procname);
			else
				priv_delete_address(name, ioctlfd, imsg.data);
			break;

		case IMSG_SET_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_set_address))
				log_warnx("%s: bad IMSG_SET_ADDRESS",
				    log_procname);
			else
				priv_set_address(name, ioctlfd, imsg.data);
			break;

		case IMSG_FLUSH_ROUTES:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_flush_routes))
				log_warnx("%s: bad IMSG_FLUSH_ROUTES",
				    log_procname);
			else
				priv_flush_routes(index, routefd, rdomain,
				    imsg.data);
			break;

		case IMSG_ADD_ROUTE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_add_route))
				log_warnx("%s: bad IMSG_ADD_ROUTE",
				    log_procname);
			else
				priv_add_route(name, rdomain, routefd,
				    imsg.data);
			break;

		case IMSG_SET_MTU:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_set_mtu))
				log_warnx("%s: bad IMSG_SET_MTU", log_procname);
			else
				priv_set_mtu(name, ioctlfd, imsg.data);
			break;

		case IMSG_SET_RESOLV_CONF:
			if (imsg.hdr.len < IMSG_HEADER_SIZE)
				log_warnx("%s: bad IMSG_SET_RESOLV_CONF",
				    log_procname);
			else {
				free(resolv_conf);
				resolv_conf = NULL;
				sz = imsg.hdr.len - IMSG_HEADER_SIZE;
				if (sz > 0) {
					resolv_conf = malloc(sz);
					if (resolv_conf == NULL)
						log_warn("%s: resolv_conf",
						    log_procname);
					else
						strlcpy(resolv_conf,
						    imsg.data, sz);
				}
				lastidx = 0;
			}
			break;

		case IMSG_WRITE_RESOLV_CONF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE)
				log_warnx("%s: bad IMSG_WRITE_RESOLV_CONF",
				    log_procname);
			else {
				retries = 0;
				do {
					newidx = default_route_index(rdomain,
					    routefd);
					retries++;
				} while (newidx == 0 && retries < 3);
				if (newidx == index && newidx != lastidx)
					priv_write_resolv_conf(resolv_conf);
				lastidx = newidx;
			}
			break;

		case IMSG_HUP:
			if (imsg.hdr.len != IMSG_HEADER_SIZE)
				log_warnx("%s: bad IMSG_HUP", log_procname);
			else
				quit = SIGHUP;
			break;

		default:
			log_warnx("%s: received unknown message, code %u",
			    log_procname, imsg.hdr.type);
		}

		imsg_free(&imsg);
	}
}

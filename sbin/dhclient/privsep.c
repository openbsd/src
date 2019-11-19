/*	$OpenBSD: privsep.c,v 1.76 2019/11/19 14:35:08 krw Exp $ */

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
#include <resolv.h>
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
	int		 index;

	index = if_nametoindex(name);
	if (index == 0) {
		log_warnx("%s: unknown interface", log_procname);
		quit = TERMINATE;
		return;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("imsg_get");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_REVOKE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_revoke))
				log_warnx("%s: bad IMSG_REVOKE",
				    log_procname);
			else
				priv_revoke_proposal(name, ioctlfd, imsg.data,
				    &resolv_conf);
			break;

		case IMSG_PROPOSE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_propose))
				log_warnx("%s: bad IMSG_PROPOSE",
				    log_procname);
			else {
				priv_propose(name, ioctlfd, imsg.data,
				    &resolv_conf, routefd, rdomain, index);
				lastidx = 0; /* Next IMSG_WRITE_RESOLV_CONF */
			}
			break;

		case IMSG_WRITE_RESOLV_CONF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE)
				log_warnx("%s: bad IMSG_WRITE_RESOLV_CONF",
				    log_procname);
			else
				priv_write_resolv_conf(index, routefd, rdomain,
				    resolv_conf, &lastidx);
			break;

		case IMSG_TELL_UNWIND:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct imsg_tell_unwind))
				log_warnx("%s: bad IMSG_TELL_UNWIND",
				    log_procname);
			else
				priv_tell_unwind(index, routefd, rdomain, imsg.data);
			break;

		default:
			log_warnx("%s: received unknown message, code %u",
			    log_procname, imsg.hdr.type);
		}

		imsg_free(&imsg);
	}
}

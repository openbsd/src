/*	$OpenBSD: packet.c,v 1.6 2019/05/05 21:33:00 akoshibe Exp $	*/

/*
 * Copyright (c) 2013-2016 Reyk Floeter <reyk@openbsd.org>
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

#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pwd.h>
#include <event.h>

#include "switchd.h"

const uint8_t etherbroadcastaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
const uint8_t etherzeroaddr[] = { 0, 0, 0, 0, 0, 0 };

int	 packet_ether_unicast(uint8_t *);

int
packet_ether_unicast(uint8_t *ea)
{
	if (memcmp(ea, etherbroadcastaddr, ETHER_ADDR_LEN) == 0 ||
	    memcmp(ea, etherzeroaddr, ETHER_ADDR_LEN) == 0 ||
	    ETHER_IS_MULTICAST(ea))
		return (-1);
	return (0);
}

int
packet_ether_input(struct ibuf *ibuf, size_t len, struct packet *pkt)
{
	struct ether_header	*eh;

	if (len < sizeof(*eh))
		return (-1);

	pkt->pkt_len = ibuf_dataleft(ibuf);
	if ((eh = ibuf_getdata(ibuf, sizeof(*eh))) == NULL) {
		log_debug("short packet");
		return (-1);
	}

	pkt->pkt_eh = eh;
	pkt->pkt_buf = (uint8_t *)eh;

	return (0);
}

int
packet_input(struct switchd *sc, struct switch_control *sw, uint32_t srcport,
    uint32_t *dstport, struct packet *pkt)
{
	struct ether_header	*eh;
	struct macaddr		*src, *dst;

	if (sw == NULL)
		return (-1);

	eh = pkt->pkt_eh;
	if ((packet_ether_unicast(eh->ether_shost) == -1) ||
	    (src = switch_learn(sc, sw, eh->ether_shost, srcport)) == NULL)
		return (-1);

	if (packet_ether_unicast(eh->ether_dhost) == -1)
		dst = NULL;
	else
		dst = switch_cached(sw, eh->ether_dhost);

	log_debug("%s: %s -> %s, port %u -> %u", __func__,
	    print_ether(eh->ether_shost),
	    print_ether(eh->ether_dhost),
	    src->mac_port,
	    dst == NULL ? OFP_PORT_ANY : dst->mac_port);

	if (dstport)
		*dstport = dst == NULL ? OFP_PORT_ANY : dst->mac_port;

	return (0);
}

/*	$OpenBSD: hello.c,v 1.4 2005/02/09 15:51:30 claudio Exp $ */

/*
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

extern struct ospfd_conf	*oeconf;

/* hello packet handling */
int
send_hello(struct iface *iface)
{
	struct sockaddr_in	 dst;
	struct hello_hdr	*hello;
	struct nbr		*nbr;
	char			*buf;
	char			*ptr;
	int			 ret = 0;

	if (iface->passive)
		return (0);

	/* XXX use buffer API instead for better decoupling */
	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("send_hello");

	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);

	switch (iface->type) {
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_BROADCAST:
		inet_aton(AllSPFRouters, &dst.sin_addr);
		break;
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
	case IF_TYPE_VIRTUALLINK:
		/* XXX these networks need to be preconfigured */
		/* dst.sin_addr.s_addr = nbr->addr.s_addr; */
		inet_aton(AllSPFRouters, &dst.sin_addr);
		break;
	default:
		fatalx("send_hello: unknown interface type");
	}

	/* OSPF header */
	gen_ospf_hdr(ptr, iface, PACKET_TYPE_HELLO);
	ptr += sizeof(struct ospf_hdr);

	/* hello header */
	hello = (struct hello_hdr *)ptr;
	hello->mask = iface->mask.s_addr;
	hello->hello_interval = htons(iface->hello_interval);
	hello->opts = oeconf->options;
	hello->rtr_priority = iface->priority;
	hello->rtr_dead_interval = htonl(iface->dead_interval);

	if (iface->dr) {
		hello->d_rtr = iface->dr->addr.s_addr;
		iface->self->dr.s_addr = iface->dr->addr.s_addr;
	}
	if (iface->bdr) {
		hello->bd_rtr = iface->bdr->addr.s_addr;
		iface->self->bdr.s_addr = iface->bdr->addr.s_addr;
	}
	ptr += sizeof(*hello);

	/* active neighbor(s) */
	LIST_FOREACH(nbr, &iface->nbr_list, entry) {
		if (ptr - buf > iface->mtu - PACKET_HDR) {
			log_warnx("send_hello: too many neighbors on "
			    "interface %s", iface->name);
			break;
		}
		if ((nbr->state >= NBR_STA_INIT) && (nbr != iface->self)) {
			memcpy(ptr, &nbr->id, sizeof(nbr->id));
			ptr += sizeof(nbr->id);
		}
	}

	/* update authentication and calculate checksum */
	auth_gen(buf, ptr - buf, iface);

	if ((ret = send_packet(iface, buf, (ptr - buf), &dst)) == -1)
		log_warnx("send_hello: error sending packet on "
		    "interface %s", iface->name);

	free(buf);
	return (ret);
}

void
recv_hello(struct iface *iface, struct in_addr src, u_int32_t rtr_id, char *buf,
    u_int16_t len)
{
	struct hello_hdr	 hello;
	struct nbr		*nbr = NULL;
	u_int32_t		 nbr_id;
	int			 twoway = 0, nbr_change = 0;

	if (len < sizeof(hello) && (len & 0x03)) {
		log_warnx("recv_hello: bad packet size, interface %s",
		    iface->name);
		return;
	}

	memcpy(&hello, buf, sizeof(hello));
	buf += sizeof(hello);
	len -= sizeof(hello);

	if (iface->type != IF_TYPE_POINTOPOINT &&
	    iface->type != IF_TYPE_VIRTUALLINK)
		if (hello.mask != iface->mask.s_addr) {
			log_warnx("recv_hello: invalid netmask, interface %s",
			    iface->name);
			return;
		}

	if (ntohs(hello.hello_interval) != iface->hello_interval) {
		log_warnx("recv_hello: invalid hello-interval %d, "
		    "interface %s", ntohs(hello.hello_interval),
		    iface->name);
		return;
	}

	if (ntohl(hello.rtr_dead_interval) != iface->dead_interval) {
		log_warnx("recv_hello: invalid router-dead-interval %d, "
		    "interface %s", ntohl(hello.rtr_dead_interval),
		    iface->name);
		return;
	}

	if ((hello.opts & OSPF_OPTION_E && iface->area->stub) ||
	    ((hello.opts & OSPF_OPTION_E) == 0 && !iface->area->stub)) {
		log_warnx("recv_hello: ExternalRoutingCapability mismatch, "
		    "interface %s", iface->name);
		return;
	}

	switch (iface->type) {
	case IF_TYPE_POINTOPOINT:
	case IF_TYPE_VIRTUALLINK:
		/* match router-id */
		LIST_FOREACH(nbr, &iface->nbr_list, entry) {
			if (nbr == iface->self)
				continue;
			if (nbr->id.s_addr == rtr_id)
				break;
		}
		break;
	case IF_TYPE_BROADCAST:
	case IF_TYPE_NBMA:
	case IF_TYPE_POINTOMULTIPOINT:
		/* match src IP */
		LIST_FOREACH(nbr, &iface->nbr_list, entry) {
			if (nbr == iface->self)
				continue;
			if (nbr->addr.s_addr == src.s_addr)
				break;
		}
		break;
	default:
		fatalx("recv_hello: unknown interface type");
	}

	if (!nbr)
		nbr = nbr_new(rtr_id, iface, 0);
	/* actually the neighbor address shouldn't be stored on virtual links */
	nbr->addr.s_addr = src.s_addr;
	nbr->options = hello.opts;

	nbr_fsm(nbr, NBR_EVT_HELLO_RCVD);

	while (len >= sizeof(nbr_id)) {
		memcpy(&nbr_id, buf, sizeof(nbr_id));
		if (nbr_id == iface->rtr_id.s_addr) {
			/* seen myself */
			if (nbr->state < NBR_STA_XSTRT)
				twoway = 1;
			break;
		}
		buf += sizeof(nbr_id);
		len -= sizeof(nbr_id);
	}

	if (len == 0) {
		nbr_fsm(nbr, NBR_EVT_1_WAY_RCVD);
		/* set neighbor parameters */
		nbr->dr.s_addr = hello.d_rtr;
		nbr->bdr.s_addr = hello.bd_rtr;
		nbr->priority = hello.rtr_priority;
		return;
	}

	if (nbr->priority != hello.rtr_priority) {
		nbr->priority = hello.rtr_priority;
		nbr_change = 1;
	}

	if (iface->state == IF_STA_WAITING &&
	    ((nbr->dr.s_addr == nbr->addr.s_addr &&
	    nbr->bdr.s_addr == 0) || nbr->bdr.s_addr == nbr->addr.s_addr))
		if_fsm(iface, IF_EVT_BACKUP_SEEN);

	if ((nbr->addr.s_addr == nbr->dr.s_addr &&
	    nbr->addr.s_addr != hello.d_rtr) ||
	    (nbr->addr.s_addr != nbr->dr.s_addr &&
	    nbr->addr.s_addr == hello.d_rtr))
		/* neighbor changed from or to DR */
		nbr_change = 1;
	if ((nbr->addr.s_addr == nbr->bdr.s_addr &&
	    nbr->addr.s_addr != hello.bd_rtr) ||
	    (nbr->addr.s_addr != nbr->bdr.s_addr &&
	    nbr->addr.s_addr == hello.bd_rtr))
		/* neighbor changed from or to BDR */
		nbr_change = 1;

	nbr->dr.s_addr = hello.d_rtr;
	nbr->bdr.s_addr = hello.bd_rtr;

	if (twoway) {
		/*
		 * event 2 way received needs to be delayed after the
		 * interface neighbor change check else the DR and BDR
		 * may not be set correctly.
		 */
		nbr_fsm(nbr, NBR_EVT_2_WAY_RCVD);
	}

	if (nbr_change)
		if_fsm(iface, IF_EVT_NBR_CHNG);

	/* TODO NBMA needs some special handling */
	return;
}

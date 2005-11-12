/*	$OpenBSD: packet.c,v 1.18 2005/11/12 18:18:24 deraadt Exp $ */

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

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

int		 ip_hdr_sanity_check(const struct ip *, u_int16_t);
int		 ospf_hdr_sanity_check(const struct ip *,
		    struct ospf_hdr *, u_int16_t, const struct iface *);
struct iface	*find_iface(struct ospfd_conf *, struct in_addr);

int
gen_ospf_hdr(struct buf *buf, struct iface *iface, u_int8_t type)
{
	struct ospf_hdr	ospf_hdr;

	bzero(&ospf_hdr, sizeof(ospf_hdr));
	ospf_hdr.version = OSPF_VERSION;
	ospf_hdr.type = type;
	ospf_hdr.rtr_id = ospfe_router_id();
	if (iface->type != IF_TYPE_VIRTUALLINK)
		ospf_hdr.area_id = iface->area->id.s_addr;
	ospf_hdr.auth_type = htons(iface->auth_type);

	return (buf_add(buf, &ospf_hdr, sizeof(ospf_hdr)));
}

/* send and receive packets */
int
send_packet(struct iface *iface, char *pkt, int len, struct sockaddr_in *dst)
{
	/* set outgoing interface for multicast traffic */
	if (IN_MULTICAST(ntohl(dst->sin_addr.s_addr)))
		if (if_set_mcast(iface) == -1) {
			log_warn("send_packet: error setting multicast "
			    "interface, %s", iface->name);
			return (-1);
		}

	if (sendto(iface->fd, pkt, len, 0,
	    (struct sockaddr *)dst, sizeof(*dst)) == -1 ) {
		log_warn("send_packet: error sending packet on interface %s",
		    iface->name);
		return (-1);
	}

	return (0);
}

void
recv_packet(int fd, short event, void *bula)
{
	struct ospfd_conf	*xconf = bula;
	struct ip		 ip_hdr;
	struct ospf_hdr		*ospf_hdr;
	struct iface		*iface;
	struct nbr		*nbr = NULL;
	struct in_addr		 addr;
	char			*buf, *ptr;
	ssize_t			 r;
	u_int16_t		 len;
	int			 l;

	if (event != EV_READ)
		return;

	/*
	 * XXX I don't like to allocate a buffer for each packet received
	 * and freeing that buffer at the end of the function. It would be
	 * enough to allocate the buffer on startup.
	 */
	if ((ptr = buf = calloc(1, READ_BUF_SIZE)) == NULL)
		fatal("recv_packet");

	if ((r = recvfrom(fd, buf, READ_BUF_SIZE, 0, NULL, NULL)) == -1) {
		if (errno != EAGAIN && errno != EINTR)
			log_debug("recv_packet: error receiving packet");
		goto done;
	}

	len = (u_int16_t)r;

	/* IP header sanity checks */
	if (len < sizeof(ip_hdr)) {
		log_warnx("recv_packet: bad packet size");
		goto done;
	}
	memcpy(&ip_hdr, buf, sizeof(ip_hdr));
	if ((l = ip_hdr_sanity_check(&ip_hdr, len)) == -1)
		goto done;
	buf += l;
	len -= l;

	/* find a matching interface */
	if ((iface = find_iface(xconf, ip_hdr.ip_src)) == NULL) {
		log_debug("recv_packet: cannot find valid interface");
		goto done;
	}

	/*
	 * Packet needs to be sent to AllSPFRouters or AllDRouters
	 * or to the address of the interface itself.
	 * AllDRouters is only valid for DR and BDR but this is checked later.
	 */
	inet_aton(AllSPFRouters, &addr);
	if (ip_hdr.ip_dst.s_addr != addr.s_addr) {
		inet_aton(AllDRouters, &addr);
		if (ip_hdr.ip_dst.s_addr != addr.s_addr) {
			if (ip_hdr.ip_dst.s_addr != iface->addr.s_addr) {
				log_debug("recv_packet: packet sent to wrong "
				    "address %s, interface %s",
				    inet_ntoa(ip_hdr.ip_dst), iface->name);
				goto done;
			}
		}
	}

	/* OSPF header sanity checks */
	if (len < sizeof(*ospf_hdr)) {
		log_warnx("recv_packet: bad packet size");
		goto done;
	}
	ospf_hdr = (struct ospf_hdr *)buf;

	if ((l = ospf_hdr_sanity_check(&ip_hdr, ospf_hdr, len, iface)) == -1)
		goto done;

	nbr = nbr_find_id(iface, ospf_hdr->rtr_id);
	if (ospf_hdr->type != PACKET_TYPE_HELLO && nbr == NULL) {
		log_debug("recv_packet: unknown neighbor ID");
		goto done;
	}

	if (auth_validate(buf, len, iface, nbr)) {
		log_warnx("recv_packet: authentication error, "
		    "interface %s", iface->name);
		goto done;
	}

	buf += sizeof(*ospf_hdr);
	len = l - sizeof(*ospf_hdr);

	/* switch OSPF packet type */
	switch (ospf_hdr->type) {
	case PACKET_TYPE_HELLO:
		inet_aton(AllDRouters, &addr);
		if (ip_hdr.ip_dst.s_addr == addr.s_addr) {
			log_debug("recv_packet: invalid destination IP "
			     "address");
			break;
		}

		recv_hello(iface, ip_hdr.ip_src, ospf_hdr->rtr_id, buf, len);
		break;
	case PACKET_TYPE_DD:
		recv_db_description(nbr, buf, len);
		break;
	case PACKET_TYPE_LS_REQUEST:
		recv_ls_req(nbr, buf, len);
		break;
	case PACKET_TYPE_LS_UPDATE:
		recv_ls_update(nbr, buf, len);
		break;
	case PACKET_TYPE_LS_ACK:
		recv_ls_ack(nbr, buf, len);
		break;
	default:
		log_debug("recv_packet: unknown OSPF packet type, interface %s",
		    iface->name);
	}
done:
	free(ptr);
}

int
ip_hdr_sanity_check(const struct ip *ip_hdr, u_int16_t len)
{
	if (ntohs(ip_hdr->ip_len) != len) {
		log_debug("recv_packet: invalid IP packet length %u",
		    ntohs(ip_hdr->ip_len));
		return (-1);
	}

	if (ip_hdr->ip_p != IPPROTO_OSPF)
		/* this is enforced by the socket itself */
		fatalx("recv_packet: invalid IP proto");

	return (ip_hdr->ip_hl << 2);
}

int
ospf_hdr_sanity_check(const struct ip *ip_hdr, struct ospf_hdr *ospf_hdr,
    u_int16_t len, const struct iface *iface)
{
	struct in_addr		 addr;

	if (ospf_hdr->version != OSPF_VERSION) {
		log_debug("recv_packet: invalid OSPF version %d",
		    ospf_hdr->version);
		return (-1);
	}

	if (ntohs(ospf_hdr->len) > len ||
	    len <= sizeof(struct ospf_hdr)) {
		log_debug("recv_packet: invalid OSPF packet length %d",
		    ntohs(ospf_hdr->len));
		return (-1);
	}

	if (iface->type != IF_TYPE_VIRTUALLINK) {
		if (ospf_hdr->area_id != iface->area->id.s_addr) {
			addr.s_addr = ospf_hdr->area_id;
			log_debug("recv_packet: invalid area ID %s, "
			    "interface %s", inet_ntoa(addr), iface->name);
			return (-1);
		}
	} else {
		if (ospf_hdr->area_id != 0) {
			addr.s_addr = ospf_hdr->area_id;
			log_debug("recv_packet: invalid area ID %s, "
			    "interface %s", inet_ntoa(addr), iface->name);
			return (-1);
		}
	}

	if (iface->type == IF_TYPE_BROADCAST || iface->type == IF_TYPE_NBMA) {
		if (inet_aton(AllDRouters, &addr) == 0)
			fatalx("recv_packet: inet_aton");
		if (ip_hdr->ip_dst.s_addr == addr.s_addr &&
		    (iface->state & IF_STA_DRORBDR) == 0) {
			log_debug("recv_packet: invalid destination IP in "
			    "state %s, interface %s",
			    if_state_name(iface->state), iface->name);
			return (-1);
		}
	}

	return (ntohs(ospf_hdr->len));
}

struct iface *
find_iface(struct ospfd_conf *xconf, struct in_addr src)
{
	struct area	*area = NULL;
	struct iface	*iface = NULL;

	/* returned interface needs to be active */
	LIST_FOREACH(area, &xconf->area_list, entry) {
		LIST_FOREACH(iface, &area->iface_list, entry) {
			if (iface->fd > 0 &&
			    (iface->type == IF_TYPE_POINTOPOINT) &&
			    (iface->dst.s_addr == src.s_addr) &&
			    !iface->passive)
				return (iface);

			if (iface->fd > 0 && (iface->addr.s_addr &
			    iface->mask.s_addr) == (src.s_addr &
			    iface->mask.s_addr) && !iface->passive &&
			    iface->type != IF_TYPE_VIRTUALLINK) {
				return (iface);
			}
		}
	}

	LIST_FOREACH(area, &xconf->area_list, entry)
		LIST_FOREACH(iface, &area->iface_list, entry)
			if ((iface->type == IF_TYPE_VIRTUALLINK) &&
			    (src.s_addr == iface->dst.s_addr)) {
				return (iface);
			}

	return (NULL);
}

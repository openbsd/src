/*	$OpenBSD: packet.c,v 1.1 2007/10/08 10:44:50 norby Exp $ */

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
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <arpa/inet.h>
#include <net/if_dl.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ospf6d.h"
#include "ospf6.h"
#include "log.h"
#include "ospfe.h"

int		 ip_hdr_sanity_check(const struct ip6_hdr *, u_int16_t);
int		 ospf_hdr_sanity_check(const struct ip6_hdr *,
		    struct ospf_hdr *, u_int16_t, const struct iface *);
struct iface	*find_iface(struct ospfd_conf *, unsigned int, struct in6_addr);

int
gen_ospf_hdr(struct buf *buf, struct iface *iface, u_int8_t type)
{
	struct ospf_hdr	ospf_hdr;

	bzero(&ospf_hdr, sizeof(ospf_hdr));
	ospf_hdr.version = OSPF6_VERSION;
	ospf_hdr.type = type;
	ospf_hdr.rtr_id = ospfe_router_id();
	if (iface->type != IF_TYPE_VIRTUALLINK)
		ospf_hdr.area_id = iface->area->id.s_addr;
	ospf_hdr.instance = DEFAULT_INSTANCE_ID;
	ospf_hdr.zero = 0;		/* must be zero */

	return (buf_add(buf, &ospf_hdr, sizeof(ospf_hdr)));
}

int
upd_ospf_hdr(struct buf *buf, struct iface *iface)
{
	struct ospf_hdr	*ospf_hdr;

	if ((ospf_hdr = buf_seek(buf, 0, sizeof(ospf_hdr))) == NULL)
		fatalx("upd_ospf_hdr: buf_seek failed");

	/* update length */
	if (buf->wpos > USHRT_MAX)
		fatalx("upd_ospf_hdr: resulting ospf packet too big");
	ospf_hdr->len = htons((u_int16_t)buf->wpos);

		ospf_hdr->chksum = in_cksum(buf->buf, buf->wpos);	/* XXX */

	return (0);
}

/* send and receive packets */
int
send_packet(struct iface *iface, void *pkt, size_t len,
    struct sockaddr_in6 *dst)
{
	struct msghdr		 msg;
	struct iovec		 iov[1];
	/* setup buffer */
	bzero(&msg, sizeof(msg));
	iov[0].iov_base = pkt;
	iov[0].iov_len = len;
	msg.msg_name = dst;
	msg.msg_namelen = sizeof(*dst);
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	/* set outgoing interface for multicast traffic */
	if (IN6_IS_ADDR_MULTICAST(&dst->sin6_addr))
		if (if_set_mcast(iface) == -1) {
			log_warn("send_packet: error setting multicast "
			    "interface, %s", iface->name);
			return (-1);
		}


	log_debug("send_packet: iface %d addr %s", iface->ifindex,
	    log_in6addr(&iface->addr));
	log_debug("send_packet: dest %s", log_in6addr(&dst->sin6_addr));
	if (sendmsg(iface->fd, &msg, MSG_DONTROUTE) == -1) {
		log_warn("send_packet: error sending packet on interface %s",
		    iface->name);
		return (-1);
	}

	return (0);
}

void
recv_packet(int fd, short event, void *bula)
{
	char			 cbuf[CMSG_SPACE(sizeof(struct sockaddr_dl))];
	struct msghdr		 msg;
	struct iovec		 iov;
	struct ip6_hdr		 ip_hdr;
	struct in6_addr		 addr;
	struct ospfd_conf	*xconf = bula;
	struct ospf_hdr		*ospf_hdr;
	struct iface		*iface;
	struct nbr		*nbr = NULL;
	char			*buf;
	struct cmsghdr		*cmsg;
	ssize_t			 r;
	u_int16_t		 len;
	int			 l;
	unsigned int		 ifindex = 0;

	if (event != EV_READ)
		return;

	/* setup buffer */
	bzero(&msg, sizeof(msg));
	iov.iov_base = buf = pkt_ptr;
	iov.iov_len = READ_BUF_SIZE;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = cbuf;
	msg.msg_controllen = sizeof(cbuf);

	if ((r = recvmsg(fd, &msg, 0)) == -1) {
		if (errno != EAGAIN && errno != EINTR)
			log_debug("recv_packet: read error: %s",
			    strerror(errno));
		return;
	}
	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IPV6 &&
		    cmsg->cmsg_type == IP_RECVIF) {
			ifindex = ((struct sockaddr_dl *)
			    CMSG_DATA(cmsg))->sdl_index;
			break;
		}
	}

	len = (u_int16_t)r;

	/* IP header sanity checks */
	if (len < sizeof(ip_hdr)) {
		log_warnx("recv_packet: bad packet size");
		return;
	}
	memcpy(&ip_hdr, buf, sizeof(ip_hdr));
	if ((l = ip_hdr_sanity_check(&ip_hdr, len)) == -1)
		return;
	buf += l;
	len -= l;

	/* find a matching interface */
	if ((iface = find_iface(xconf, ifindex, ip_hdr.ip6_src)) == NULL) {
		/* XXX add a counter here */
		return;
	}
#if 1
	/*
	 * Packet needs to be sent to AllSPFRouters or AllDRouters
	 * or to the address of the interface itself.
	 * AllDRouters is only valid for DR and BDR but this is checked later.
	 */
	inet_pton(AF_INET6, AllSPFRouters, &addr);

	if (!IN6_ARE_ADDR_EQUAL(&ip_hdr.ip6_dst, &addr)) {
		inet_pton(AF_INET6, AllDRouters, &addr);
		if (!IN6_ARE_ADDR_EQUAL(&ip_hdr.ip6_dst, &addr)) {
			if (!IN6_ARE_ADDR_EQUAL(&ip_hdr.ip6_dst,
			     &iface->addr)) {
				log_debug("recv_packet: packet sent to wrong "
				    "address %s, interface %s",
				    log_in6addr(&ip_hdr.ip6_dst), iface->name);
				return;
			}
		}
	}
#endif
	/* OSPF header sanity checks */
	if (len < sizeof(*ospf_hdr)) {
		log_debug("recv_packet: bad packet size");
		return;
	}
	ospf_hdr = (struct ospf_hdr *)buf;

	if ((l = ospf_hdr_sanity_check(&ip_hdr, ospf_hdr, len, iface)) == -1)
		return;

	nbr = nbr_find_id(iface, ospf_hdr->rtr_id);
	if (ospf_hdr->type != PACKET_TYPE_HELLO && nbr == NULL) {
		log_debug("recv_packet: unknown neighbor ID");
		return;
	}

	if (in_cksum(buf, len)) {
		log_warnx("recv_packet: invalid checksum, "
		    "interface %s", iface->name);
		return;
	}

	buf += sizeof(*ospf_hdr);
	len = l - sizeof(*ospf_hdr);

	/* switch OSPF packet type */
	switch (ospf_hdr->type) {
	case PACKET_TYPE_HELLO:
		inet_pton(AF_INET6, AllDRouters, &addr);
		if (IN6_ARE_ADDR_EQUAL(&ip_hdr.ip6_dst, &addr)) {
			log_debug("recv_packet: invalid destination IP "
			     "address");
			break;
		}

		recv_hello(iface, ip_hdr.ip6_src, ospf_hdr->rtr_id, buf, len);
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
}

int
ip_hdr_sanity_check(const struct ip6_hdr *ip_hdr, u_int16_t len)
{
	if (ntohs(ip_hdr->ip6_plen) != len) {
		log_debug("recv_packet: invalid IP packet length %u",
		    ntohs(ip_hdr->ip6_plen));
		return (-1);
	}

	if (ip_hdr->ip6_nxt != IPPROTO_OSPF)
		/* this is enforced by the socket itself */
		fatalx("recv_packet: invalid IP proto");

//XXX	return (ip_hdr->ip_hl << 2);
	return (sizeof(struct ip6_hdr));
}

int
ospf_hdr_sanity_check(const struct ip6_hdr *ip_hdr, struct ospf_hdr *ospf_hdr,
    u_int16_t len, const struct iface *iface)
{
	struct in6_addr		 addr;
	struct in_addr		 id;

	if (ospf_hdr->version != OSPF6_VERSION) {
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
			id.s_addr = ospf_hdr->area_id;
			log_debug("recv_packet: invalid area ID %s, "
			    "interface %s", inet_ntoa(id), iface->name);
			return (-1);
		}
	} else {
		if (ospf_hdr->area_id != 0) {
			id.s_addr = ospf_hdr->area_id;
			log_debug("recv_packet: invalid area ID %s, "
			    "interface %s", inet_ntoa(id), iface->name);
			return (-1);
		}
	}

	if (iface->type == IF_TYPE_BROADCAST || iface->type == IF_TYPE_NBMA) {
		if (inet_pton(AF_INET6, AllDRouters, &addr) == 0)
			fatalx("recv_packet: inet_pton");
		if (IN6_ARE_ADDR_EQUAL(&ip_hdr->ip6_dst, &addr) &&
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
find_iface(struct ospfd_conf *xconf, unsigned int ifindex, struct in6_addr src)
{
	struct area	*area = NULL;
	struct iface	*iface = NULL;

	/* returned interface needs to be active */
	LIST_FOREACH(area, &xconf->area_list, entry) {
		LIST_FOREACH(iface, &area->iface_list, entry) {
			switch (iface->type) {
			case IF_TYPE_VIRTUALLINK:
				if (IN6_ARE_ADDR_EQUAL(&src, &iface->dst) &&
				    !iface->passive)
					return (iface);
				break;
			case IF_TYPE_POINTOPOINT:
				if (ifindex == iface->ifindex &&
				    IN6_ARE_ADDR_EQUAL(&iface->dst, &src) &&
				    !iface->passive)
					return (iface);
				break;
			default:
#if 0
				if (ifindex == iface->ifindex &&
				    (iface->addr.s_addr & iface->mask.s_addr) ==
				    (src.s_addr & iface->mask.s_addr) &&
				    !iface->passive)
					return (iface);
#endif
				break;
			}
		}
	}

	return (NULL);
}

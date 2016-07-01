/*	$OpenBSD: address.c,v 1.27 2016/07/01 23:29:55 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
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
#include <arpa/inet.h>
#include <string.h>

#include "ldpd.h"
#include "ldpe.h"
#include "lde.h"
#include "log.h"

static int	gen_address_list_tlv(struct ibuf *, uint16_t, int,
		    struct if_addr *);

void
send_address(struct nbr *nbr, int af, struct if_addr *if_addr, int withdraw)
{
	struct ibuf	*buf;
	uint32_t	 msg_type;
	uint16_t	 size;
	int		 iface_count = 0;
	int		 err = 0;

	if (!withdraw)
		msg_type = MSG_TYPE_ADDR;
	else
		msg_type = MSG_TYPE_ADDRWITHDRAW;

	if (if_addr == NULL) {
		LIST_FOREACH(if_addr, &global.addr_list, entry)
			if (if_addr->af == af)
				iface_count++;
	} else
		iface_count = 1;

	size = LDP_HDR_SIZE + LDP_MSG_SIZE + sizeof(struct address_list_tlv);
	switch (af) {
	case AF_INET:
		size += iface_count * sizeof(struct in_addr);
		break;
	case AF_INET6:
		size += iface_count * sizeof(struct in6_addr);
		break;
	default:
		fatalx("send_address: unknown af");
	}

	if ((buf = ibuf_open(size)) == NULL)
		fatal(__func__);

	err |= gen_ldp_hdr(buf, size);
	size -= LDP_HDR_SIZE;
	err |= gen_msg_hdr(buf, msg_type, size);
	size -= LDP_MSG_SIZE;
	err |= gen_address_list_tlv(buf, size, af, if_addr);
	if (err) {
		ibuf_free(buf);
		return;
	}

	evbuf_enqueue(&nbr->tcp->wbuf, buf);

	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

int
recv_address(struct nbr *nbr, char *buf, uint16_t len)
{
	struct ldp_msg		addr;
	struct address_list_tlv	alt;
	enum imsg_type		type;
	struct lde_addr		lde_addr;

	memcpy(&addr, buf, sizeof(addr));
	buf += LDP_MSG_SIZE;
	len -= LDP_MSG_SIZE;

	/* Address List TLV */
	if (len < sizeof(alt)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, addr.msgid, addr.type);
		return (-1);
	}

	memcpy(&alt, buf, sizeof(alt));
	if (ntohs(alt.length) != len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, addr.msgid, addr.type);
		return (-1);
	}
	if (ntohs(alt.type) != TLV_TYPE_ADDRLIST) {
		session_shutdown(nbr, S_UNKNOWN_TLV, addr.msgid, addr.type);
		return (-1);
	}
	switch (ntohs(alt.family)) {
	case AF_IPV4:
		if (!nbr->v4_enabled)
			/* just ignore the message */
			return (0);
		break;
	case AF_IPV6:
		if (!nbr->v6_enabled)
			/* just ignore the message */
			return (0);
		break;
	default:
		send_notification_nbr(nbr, S_UNSUP_ADDR, addr.msgid, addr.type);
		return (-1);
	}
	buf += sizeof(alt);
	len -= sizeof(alt);

	if (ntohs(addr.type) == MSG_TYPE_ADDR)
		type = IMSG_ADDRESS_ADD;
	else
		type = IMSG_ADDRESS_DEL;

	while (len > 0) {
		switch (ntohs(alt.family)) {
		case AF_IPV4:
			if (len < sizeof(struct in_addr)) {
				session_shutdown(nbr, S_BAD_TLV_LEN, addr.msgid,
				    addr.type);
				return (-1);
			}

			memset(&lde_addr, 0, sizeof(lde_addr));
			lde_addr.af = AF_INET;
			memcpy(&lde_addr.addr, buf, sizeof(struct in_addr));

			buf += sizeof(struct in_addr);
			len -= sizeof(struct in_addr);
			break;
		case AF_IPV6:
			if (len < sizeof(struct in6_addr)) {
				session_shutdown(nbr, S_BAD_TLV_LEN, addr.msgid,
				    addr.type);
				return (-1);
			}

			memset(&lde_addr, 0, sizeof(lde_addr));
			lde_addr.af = AF_INET6;
			memcpy(&lde_addr.addr, buf, sizeof(struct in6_addr));

			buf += sizeof(struct in6_addr);
			len -= sizeof(struct in6_addr);
			break;
		default:
			fatalx("recv_address: unknown af");
		}

		log_debug("%s: lsr-id %s address %s%s", __func__,
		    inet_ntoa(nbr->id), log_addr(lde_addr.af, &lde_addr.addr),
		    ntohs(addr.type) == MSG_TYPE_ADDR ? "" : " (withdraw)");

		ldpe_imsg_compose_lde(type, nbr->peerid, 0, &lde_addr,
		    sizeof(lde_addr));
	}

	return (0);
}

static int
gen_address_list_tlv(struct ibuf *buf, uint16_t size, int af,
    struct if_addr *if_addr)
{
	struct address_list_tlv	 alt;
	uint16_t		 addr_size;
	int			 err = 0;

	memset(&alt, 0, sizeof(alt));
	alt.type = TLV_TYPE_ADDRLIST;
	alt.length = htons(size - TLV_HDR_LEN);
	switch (af) {
	case AF_INET:
		alt.family = htons(AF_IPV4);
		addr_size = sizeof(struct in_addr);
		break;
	case AF_INET6:
		alt.family = htons(AF_IPV6);
		addr_size = sizeof(struct in6_addr);
		break;
	default:
		fatalx("gen_address_list_tlv: unknown af");
	}

	err |= ibuf_add(buf, &alt, sizeof(alt));
	if (if_addr == NULL) {
		LIST_FOREACH(if_addr, &global.addr_list, entry) {
			if (if_addr->af != af)
				continue;
			err |= ibuf_add(buf, &if_addr->addr, addr_size);
		}
	} else
		err |= ibuf_add(buf, &if_addr->addr, addr_size);

	return (err);
}

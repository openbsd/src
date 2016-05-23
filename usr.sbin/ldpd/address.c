/*	$OpenBSD: address.c,v 1.21 2016/05/23 17:43:42 renato Exp $ */

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
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <unistd.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

extern struct ldpd_conf        *leconf;

void	gen_address_list_tlv(struct ibuf *, struct if_addr *, uint16_t);

void
send_address(struct nbr *nbr, struct if_addr *if_addr, int withdraw)
{
	struct ibuf	*buf;
	uint32_t	 msg_type;
	uint16_t	 size;
	int		 iface_count = 0;

	if (!withdraw)
		msg_type = MSG_TYPE_ADDR;
	else
		msg_type = MSG_TYPE_ADDRWITHDRAW;

	if (if_addr == NULL) {
		LIST_FOREACH(if_addr, &global.addr_list, entry)
			iface_count++;
	} else
		iface_count = 1;

	size = LDP_HDR_SIZE + LDP_MSG_SIZE + sizeof(struct address_list_tlv) +
	    iface_count * sizeof(struct in_addr);

	if ((buf = ibuf_open(size)) == NULL)
		fatal(__func__);

	gen_ldp_hdr(buf, size);
	size -= LDP_HDR_SIZE;
	gen_msg_hdr(buf, msg_type, size);
	size -= LDP_MSG_SIZE;
	gen_address_list_tlv(buf, if_addr, size);

	evbuf_enqueue(&nbr->tcp->wbuf, buf);

	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

int
recv_address(struct nbr *nbr, char *buf, uint16_t len)
{
	struct ldp_msg		addr;
	struct address_list_tlv	alt;
	enum imsg_type		type;

	memcpy(&addr, buf, sizeof(addr));
	log_debug("%s: lsr-id %s%s", __func__, inet_ntoa(nbr->id),
	    ntohs(addr.type) == MSG_TYPE_ADDR ? "" : " address withdraw");
	if (ntohs(addr.type) == MSG_TYPE_ADDR)
		type = IMSG_ADDRESS_ADD;
	else
		type = IMSG_ADDRESS_DEL;

	buf += LDP_MSG_SIZE;
	len -= LDP_MSG_SIZE;

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

	/* For now we only support IPv4 */
	if (alt.family != htons(AF_IPV4)) {
		send_notification_nbr(nbr, S_UNSUP_ADDR, addr.msgid, addr.type);
		return (-1);
	}

	buf += sizeof(alt);
	len -= sizeof(alt);

	while (len >= sizeof(struct in_addr)) {
		ldpe_imsg_compose_lde(type, nbr->peerid, 0,
		    buf, sizeof(struct in_addr));

		buf += sizeof(struct in_addr);
		len -= sizeof(struct in_addr);
	}

	if (len != 0) {
		session_shutdown(nbr, S_BAD_TLV_LEN, addr.msgid, addr.type);
		return (-1);
	}

	return (0);
}

void
gen_address_list_tlv(struct ibuf *buf, struct if_addr *if_addr, uint16_t size)
{
	struct address_list_tlv	 alt;


	memset(&alt, 0, sizeof(alt));
	alt.type = TLV_TYPE_ADDRLIST;
	alt.length = htons(size - TLV_HDR_LEN);
	/* XXX: just ipv4 for now */
	alt.family = htons(AF_IPV4);

	ibuf_add(buf, &alt, sizeof(alt));

	if (if_addr == NULL) {
		LIST_FOREACH(if_addr, &global.addr_list, entry)
			ibuf_add(buf, &if_addr->addr, sizeof(if_addr->addr));
	} else
		ibuf_add(buf, &if_addr->addr, sizeof(if_addr->addr));
}

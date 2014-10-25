/*	$OpenBSD: address.c,v 1.15 2014/10/25 03:23:49 lteo Exp $ */

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

void	gen_address_list_tlv(struct ibuf *, struct if_addr *, u_int16_t);

void
send_address(struct nbr *nbr, struct if_addr *if_addr)
{
	struct ibuf	*buf;
	u_int16_t	 size, iface_count = 0;

	log_debug("send_address: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_address");

	if (if_addr == NULL)
		LIST_FOREACH(if_addr, &leconf->addr_list, global_entry)
			iface_count++;
	else
		iface_count = 1;

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct address_list_tlv) +
	    iface_count * sizeof(struct in_addr);

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_ADDR, size);

	size -= sizeof(struct ldp_msg);

	gen_address_list_tlv(buf, if_addr, size);

	evbuf_enqueue(&nbr->tcp->wbuf, buf);
	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

int
recv_address(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		addr;
	struct address_list_tlv	alt;
	enum imsg_type		type;

	bcopy(buf, &addr, sizeof(addr));
	log_debug("recv_address: neighbor ID %s%s", inet_ntoa(nbr->id),
	    ntohs(addr.type) == MSG_TYPE_ADDR ? "" : " address withdraw");
	if (ntohs(addr.type) == MSG_TYPE_ADDR)
		type = IMSG_ADDRESS_ADD;
	else
		type = IMSG_ADDRESS_DEL;

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	if (len < sizeof(alt)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, addr.msgid, addr.type);
		return (-1);
	}

	bcopy(buf, &alt, sizeof(alt));

	if (ntohs(alt.length) != len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, addr.msgid, addr.type);
		return (-1);
	}

	if (ntohs(alt.type) != TLV_TYPE_ADDRLIST) {
		session_shutdown(nbr, S_UNKNOWN_TLV, addr.msgid, addr.type);
		return (-1);
	}

	/* For now we only support IPv4 */
	if (alt.family != htons(ADDR_IPV4)) {
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

	return (ntohs(addr.length));
}

void
gen_address_list_tlv(struct ibuf *buf, struct if_addr *if_addr,
    u_int16_t size)
{
	struct address_list_tlv	 alt;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&alt, sizeof(alt));
	alt.type = TLV_TYPE_ADDRLIST;
	alt.length = htons(size);
	/* XXX: just ipv4 for now */
	alt.family = htons(ADDR_IPV4);

	ibuf_add(buf, &alt, sizeof(alt));

	if (if_addr == NULL)
		LIST_FOREACH(if_addr, &leconf->addr_list, global_entry)
			ibuf_add(buf, &if_addr->addr, sizeof(if_addr->addr));
	else
		ibuf_add(buf, &if_addr->addr, sizeof(if_addr->addr));
}

void
send_address_withdraw(struct nbr *nbr, struct if_addr *if_addr)
{
	struct ibuf	*buf;
	u_int16_t	 size;

	log_debug("send_address_withdraw: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_address_withdraw");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct address_list_tlv) + sizeof(struct in_addr);

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_ADDRWITHDRAW, size);

	size -= sizeof(struct ldp_msg);

	gen_address_list_tlv(buf, if_addr, size);

	evbuf_enqueue(&nbr->tcp->wbuf, buf);
	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

/*	$OpenBSD: address.c,v 1.6 2010/11/04 09:52:16 claudio Exp $ */

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
#include <netinet/in_systm.h>
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

void	gen_address_list_tlv(struct ibuf *, struct iface *, u_int16_t);

void
send_address(struct nbr *nbr, struct iface *iface)
{
	struct ibuf	*buf;
	struct iface	*niface;
	u_int16_t	 size, iface_count = 0;

	if (nbr->iface->passive)
		return;

	log_debug("send_address: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_address");

	/* XXX: multiple address on the same iface? */
	if (iface == NULL)
		LIST_FOREACH(niface, &leconf->iface_list, entry)
			iface_count++;
	else
		iface_count = 1;

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct address_list_tlv) +
	    iface_count * sizeof(struct in_addr);

	gen_ldp_hdr(buf, nbr->iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_ADDR, size);

	size -= sizeof(struct ldp_msg);

	gen_address_list_tlv(buf, iface, size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_address(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		addr;
	struct address_list_tlv	alt;
	enum imsg_type		type;

	if (nbr->state != NBR_STA_OPER) {
		log_debug("recv_address: neighbor ID %s not operational",
		    inet_ntoa(nbr->id));
		return (-1);
	}

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
gen_address_list_tlv(struct ibuf *buf, struct iface *iface, u_int16_t size)
{
	struct address_list_tlv	 alt;
	struct iface		*niface;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&alt, sizeof(alt));
	alt.type = TLV_TYPE_ADDRLIST;
	alt.length = htons(size);
	/* XXX: just ipv4 for now */
	alt.family = htons(ADDR_IPV4);

	ibuf_add(buf, &alt, sizeof(alt));

	if (iface == NULL)
		LIST_FOREACH(niface, &leconf->iface_list, entry)
			ibuf_add(buf, &niface->addr, sizeof(niface->addr));
	else
		ibuf_add(buf, &iface->addr, sizeof(iface->addr));
}

void
send_address_withdraw(struct nbr *nbr, struct iface *iface)
{
	struct ibuf	*buf;
	u_int16_t	 size;

	if (nbr->iface->passive)
		return;

	log_debug("send_address_withdraw: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_address_withdraw");

	/* XXX: multiple address on the same iface? */
	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) + sizeof(struct in_addr);

	gen_ldp_hdr(buf, nbr->iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_ADDRWITHDRAW, size);

	size -= sizeof(struct ldp_msg);

	gen_address_list_tlv(buf, iface, size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

/*	$OpenBSD: hello.c,v 1.8 2011/01/08 14:50:29 claudio Exp $ */

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

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

int	tlv_decode_hello_prms(char *, u_int16_t, u_int16_t *, u_int16_t *);
int	tlv_decode_opt_hello_prms(char *, u_int16_t, struct in_addr *,
	    u_int32_t *);
int	gen_hello_prms_tlv(struct iface *, struct ibuf *, u_int16_t);

int
send_hello(struct iface *iface)
{
	struct sockaddr_in	 dst;
	struct ibuf		*buf;
	u_int16_t		 size;

	dst.sin_port = htons(LDP_PORT);
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	inet_aton(AllRouters, &dst.sin_addr);

	if (iface->passive)
		return (0);

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_hello");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct hello_prms_tlv);

	gen_ldp_hdr(buf, iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_HELLO, size);

	size -= sizeof(struct ldp_msg);

	gen_hello_prms_tlv(iface, buf, size);

	send_packet(iface, buf->buf, buf->wpos, &dst);
	ibuf_free(buf);

	return (0);
}

void
recv_hello(struct iface *iface, struct in_addr src, char *buf, u_int16_t len)
{
	struct ldp_msg		 hello;
	struct ldp_hdr		 ldp;
	struct nbr		*nbr = NULL;
	struct in_addr		 address;
	u_int32_t		 conf_number;
	u_int16_t		 holdtime, flags;
	int			 r;

	bcopy(buf, &ldp, sizeof(ldp));
	buf += LDP_HDR_SIZE;
	len -= LDP_HDR_SIZE;

	bcopy(buf, &hello, sizeof(hello));
	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	r = tlv_decode_hello_prms(buf, len, &holdtime, &flags);
	if (r == -1) {
		address.s_addr = ldp.lsr_id;
		log_debug("recv_hello: neighbor %s: failed to decode params",
		    inet_ntoa(address));
		return;
	}

	buf += r;
	len -= r;

	r = tlv_decode_opt_hello_prms(buf, len, &address, &conf_number);
	if (r == -1) {
		address.s_addr = ldp.lsr_id;
		log_debug("recv_hello: neighbor %s: failed to decode "
		    "optional params", inet_ntoa(address));
		return;
	}
	if (r != len) {
		address.s_addr = ldp.lsr_id;
		log_debug("recv_hello: neighbor %s: unexpected data in message",
		    inet_ntoa(address));
		return;
	}

	nbr = nbr_find_ldpid(iface, ldp.lsr_id, ldp.lspace_id);
	if (!nbr) {
		nbr = nbr_new(ldp.lsr_id, ldp.lspace_id, iface);

		/* set neighbor parameters */
		if (address.s_addr == INADDR_ANY)
			nbr->addr.s_addr = src.s_addr;
		else
			nbr->addr.s_addr = address.s_addr;

		nbr->hello_type = flags;

		if (holdtime == 0) {
			/* XXX: lacks support for targeted hellos */
			if (iface->holdtime < LINK_DFLT_HOLDTIME)
				nbr->holdtime = iface->holdtime;
			else
				nbr->holdtime = LINK_DFLT_HOLDTIME;
		} else if (holdtime == INFINITE_HOLDTIME) {
			/* No timeout for this neighbor */
			nbr->holdtime = iface->holdtime;
		} else {
			if (iface->holdtime < holdtime)
				nbr->holdtime = iface->holdtime;
			else
				nbr->holdtime = holdtime;
		}
	}

	nbr_fsm(nbr, NBR_EVT_HELLO_RCVD);

	if (ntohl(nbr->addr.s_addr) < ntohl(nbr->iface->addr.s_addr) &&
	    nbr->state == NBR_STA_PRESENT && !nbr_pending_idtimer(nbr))
		nbr_act_session_establish(nbr, 1);
}

int
gen_hello_prms_tlv(struct iface *iface, struct ibuf *buf, u_int16_t size)
{
	struct hello_prms_tlv	parms;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&parms, sizeof(parms));
	parms.type = htons(TLV_TYPE_COMMONHELLO);
	parms.length = htons(size);
	/* XXX */
	parms.holdtime = htons(iface->holdtime);
	parms.flags = 0;

	return (ibuf_add(buf, &parms, sizeof(parms)));
}

int
tlv_decode_hello_prms(char *buf, u_int16_t len, u_int16_t *holdtime,
    u_int16_t *flags)
{
	struct hello_prms_tlv	tlv;

	if (len < sizeof(tlv))
		return (-1);
	bcopy(buf, &tlv, sizeof(tlv));

	if (ntohs(tlv.length) != sizeof(tlv) - TLV_HDR_LEN)
		return (-1);

	if (tlv.type != htons(TLV_TYPE_COMMONHELLO))
		return (-1);

	*holdtime = ntohs(tlv.holdtime);
	*flags = ntohs(tlv.flags);

	return (sizeof(tlv));
}

int
tlv_decode_opt_hello_prms(char *buf, u_int16_t len, struct in_addr *addr,
    u_int32_t *conf_number)
{
	struct hello_opt_parms_tlv	tlv;
	int				cons = 0;

	bzero(addr, sizeof(*addr));
	*conf_number = 0;

	while (len >= sizeof(tlv)) {
		bcopy(buf, &tlv, sizeof(tlv));

		if (ntohs(tlv.length) < sizeof(u_int32_t))
			return (-1);

		switch (ntohs(tlv.type)) {
		case TLV_TYPE_IPV4TRANSADDR:
			addr->s_addr = tlv.value;
			break;
		case TLV_TYPE_CONFIG:
			*conf_number = ntohl(tlv.value);
			break;
		default:
			return (-1);
		}

		len -= sizeof(tlv);
		buf += sizeof(tlv);
		cons += sizeof(tlv);
	}

	return (cons);
}

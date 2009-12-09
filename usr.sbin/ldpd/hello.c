/*	$OpenBSD: hello.c,v 1.3 2009/12/09 12:19:29 michele Exp $ */

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

struct hello_prms_tlv	*tlv_decode_hello_prms(char *, u_int16_t);
int			 tlv_decode_opt_hello_prms(char *, u_int16_t,
			    struct in_addr *, u_int32_t *);
int			 gen_hello_prms_tlv(struct iface *, struct buf *,
			    u_int16_t);

int
send_hello(struct iface *iface)
{
	struct sockaddr_in	 dst;
	struct buf		*buf;
	u_int16_t		 size;

	dst.sin_port = htons(LDP_PORT);
	dst.sin_family = AF_INET;
	dst.sin_len = sizeof(struct sockaddr_in);
	inet_aton(AllRouters, &dst.sin_addr);

	if (iface->passive)
		return (0);

	log_debug("send_hello: iface %s", iface->name);

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_hello");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) +
	    sizeof(struct hello_prms_tlv);

	gen_ldp_hdr(buf, iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_HELLO, size);

	size -= sizeof(struct ldp_msg);

	gen_hello_prms_tlv(iface, buf, size);

	send_packet(iface, buf->buf, buf->wpos, &dst);
	buf_free(buf);

	return (0);
}

void
recv_hello(struct iface *iface, struct in_addr src, char *buf, u_int16_t len)
{
	struct ldp_msg		*hello;
	u_int32_t		 messageid;
	struct nbr		*nbr = NULL;
	struct hello_prms_tlv	*cpt;
	struct ldp_hdr		*ldp;
	struct in_addr		 address;
	u_int32_t		 conf_number;

	log_debug("recv_hello: neighbor %s", inet_ntoa(src));

	ldp = (struct ldp_hdr *)buf;

	buf += LDP_HDR_SIZE;
	len -= LDP_HDR_SIZE;

	hello = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(hello->length))
		return;

	messageid = hello->msgid;

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	cpt = tlv_decode_hello_prms(buf, len);
	if (cpt == NULL)
		return;

	buf += sizeof(struct hello_prms_tlv);
	len -= sizeof(struct hello_prms_tlv);

	tlv_decode_opt_hello_prms(buf, len, &address, &conf_number);

	nbr = nbr_find_ldpid(iface, ldp->lsr_id, ldp->lspace_id);
	if (!nbr) {
		nbr = nbr_new(ldp->lsr_id, ldp->lspace_id, iface, 0);

		/* set neighbor parameters */
		if (address.s_addr == INADDR_ANY)
			nbr->addr.s_addr = src.s_addr;
		else
			nbr->addr.s_addr = address.s_addr;

		nbr->hello_type = cpt->reserved;

		if (cpt->holdtime == 0) {
			/* XXX: lacks support for targeted hellos */
			if (iface->holdtime < LINK_DFLT_HOLDTIME)
				nbr->holdtime = iface->holdtime;
			else
				nbr->holdtime = LINK_DFLT_HOLDTIME;
		} else if (cpt->holdtime == INFINITE_HOLDTIME) {
			/* No timeout for this neighbor */
			nbr->holdtime = iface->holdtime;
		} else {
			if (iface->holdtime < ntohs(cpt->holdtime))
				nbr->holdtime = iface->holdtime;
			else
				nbr->holdtime = ntohs(cpt->holdtime);
		}
	}

	nbr_fsm(nbr, NBR_EVT_HELLO_RCVD);

	if (ntohl(nbr->addr.s_addr) < ntohl(nbr->iface->addr.s_addr) &&
	    nbr->state == NBR_STA_PRESENT && !nbr_pending_idtimer(nbr))
		nbr_act_session_establish(nbr, 1);
}

int
gen_hello_prms_tlv(struct iface *iface, struct buf *buf, u_int16_t size)
{
	struct hello_prms_tlv	parms;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&parms, sizeof(parms));
	parms.type = htons(TLV_TYPE_COMMONHELLO);
	parms.length = htons(size);
	/* XXX */
	parms.holdtime = htons(iface->holdtime);
	parms.reserved = 0;

	return (buf_add(buf, &parms, sizeof(parms)));
}

struct hello_prms_tlv *
tlv_decode_hello_prms(char *buf, u_int16_t len)
{
	struct hello_prms_tlv	*tlv;

	tlv = (struct hello_prms_tlv *)buf;

	if (len < sizeof(*tlv) || ntohs(tlv->length) !=
	    (sizeof(tlv->holdtime) + sizeof(tlv->reserved)))
		return (NULL);

	if ((tlv->type & ~UNKNOWN_FLAGS_MASK) != htons(TLV_TYPE_COMMONHELLO))
		return (NULL);

	if ((tlv->type & UNKNOWN_FLAGS_MASK) != 0)
		return (NULL);

	return (tlv);
}

int
tlv_decode_opt_hello_prms(char *buf, u_int16_t len, struct in_addr *addr,
    u_int32_t *conf_number)
{
	struct hello_opt_parms_tlv	*tlv;

	bzero(addr, sizeof(*addr));
	*conf_number = 0;

	while (len >= sizeof(*tlv)) {
		tlv = (struct hello_opt_parms_tlv *)buf;

		if (tlv->length < sizeof(u_int32_t))
			return (-1);

		switch (ntohs(tlv->type)) {
		case TLV_TYPE_IPV4TRANSADDR:
			addr->s_addr = tlv->value;
			break;
		case TLV_TYPE_CONFIG:
			*conf_number = ntohl(tlv->value);
			break;
		default:
			return (-1);
		}

		len -= sizeof(*tlv);
		buf += sizeof(*tlv);
	}

	return (0);
}

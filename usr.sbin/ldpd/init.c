/*	$OpenBSD: init.c,v 1.5 2010/05/26 13:56:07 nicm Exp $ */

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

int	gen_init_prms_tlv(struct ibuf *, struct nbr *, u_int16_t);

void
send_init(struct nbr *nbr)
{
	struct ibuf		*buf;
	u_int16_t		 size;

	if (nbr->iface->passive)
		return;

	log_debug("send_init: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_init");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) + SESS_PRMS_SIZE;

	gen_ldp_hdr(buf, nbr->iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_INIT, size);

	size -= sizeof(struct ldp_msg);

	gen_init_prms_tlv(buf, nbr, size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_init(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		*init;
	struct sess_prms_tlv	*sess_tlv;

	log_debug("recv_init: neighbor ID %s", inet_ntoa(nbr->id));

	init = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(init->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, init->msgid, init->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	sess_tlv = (struct sess_prms_tlv *)buf;

	if (len < SESS_PRMS_SIZE ||
	    ntohs(sess_tlv->length) != (SESS_PRMS_SIZE - TLV_HDR_LEN)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, init->msgid, init->type);
		return (-1);
	}

	if (nbr->iface->keepalive < ntohs(sess_tlv->keepalive_time))
		nbr->keepalive = nbr->iface->keepalive;
	else
		nbr->keepalive = ntohs(sess_tlv->keepalive_time);

	nbr_fsm(nbr, NBR_EVT_INIT_RCVD);

	return (ntohs(init->length));
}

int
gen_init_prms_tlv(struct ibuf *buf, struct nbr *nbr, u_int16_t size)
{
	struct sess_prms_tlv	parms;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&parms, sizeof(parms));
	parms.type = htons(TLV_TYPE_COMMONSESSION);
	parms.length = htons(size);
	parms.proto_version = htons(LDP_VERSION);
	parms.keepalive_time = htons(nbr->iface->keepalive);
	parms.reserved = 0;
	parms.pvlim = 0;
	parms.max_pdu_len = 0;
	parms.lsr_id = nbr->id.s_addr;
	/* XXX: nbr lspace */
	parms.lspace_id = 0;

	return (ibuf_add(buf, &parms, SESS_PRMS_SIZE));
}

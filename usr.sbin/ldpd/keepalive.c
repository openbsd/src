/*	$OpenBSD: keepalive.c,v 1.4 2010/02/20 21:28:39 michele Exp $ */

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

void
send_keepalive(struct nbr *nbr)
{
	struct buf	*buf;
	u_int16_t	 size;

	if (nbr->iface->passive)
		return;

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_keepalive");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg);

	gen_ldp_hdr(buf, nbr->iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_KEEPALIVE, size);

	bufferevent_write(nbr->bev, buf->buf, buf->wpos);
	buf_free(buf);
}

int
recv_keepalive(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg *ka;

	ka = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(ka->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, ka->msgid, ka->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	if (nbr->state != NBR_STA_OPER)
		nbr_fsm(nbr, NBR_EVT_KEEPALIVE_RCVD);
	else
		nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(ka->length));
}

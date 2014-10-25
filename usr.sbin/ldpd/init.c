/*	$OpenBSD: init.c,v 1.15 2014/10/25 03:23:49 lteo Exp $ */

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

int	gen_init_prms_tlv(struct ibuf *, struct nbr *, u_int16_t);
int	tlv_decode_opt_init_prms(char *, u_int16_t);

void
send_init(struct nbr *nbr)
{
	struct ibuf		*buf;
	u_int16_t		 size;

	log_debug("send_init: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_init");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) + SESS_PRMS_SIZE;

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_INIT, size);

	size -= sizeof(struct ldp_msg);

	gen_init_prms_tlv(buf, nbr, size);

	evbuf_enqueue(&nbr->tcp->wbuf, buf);
}

int
recv_init(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		init;
	struct sess_prms_tlv	sess;

	log_debug("recv_init: neighbor ID %s", inet_ntoa(nbr->id));

	bcopy(buf, &init, sizeof(init));

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	if (len < SESS_PRMS_SIZE) {
		session_shutdown(nbr, S_BAD_MSG_LEN, init.msgid, init.type);
		return (-1);
	}
	bcopy(buf, &sess, sizeof(sess));

	if (ntohs(sess.length) != SESS_PRMS_SIZE - TLV_HDR_LEN ||
	    ntohs(sess.length) > len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, init.msgid, init.type);
		return (-1);
	}

	if (ntohs(sess.proto_version) != LDP_VERSION) {
		session_shutdown(nbr, S_BAD_PROTO_VER, init.msgid, init.type);
		return (-1);
	}

	buf += SESS_PRMS_SIZE;
	len -= SESS_PRMS_SIZE;

	/* just ignore all optional TLVs for now */
	if (tlv_decode_opt_init_prms(buf, len) == -1) {
		session_shutdown(nbr, S_BAD_TLV_VAL, init.msgid, init.type);
		return (-1);
	}

	nbr->keepalive = min(leconf->keepalive, ntohs(sess.keepalive_time));

	if (!nbr_pending_idtimer(nbr))
		nbr_fsm(nbr, NBR_EVT_INIT_RCVD);

	return (ntohs(init.length));
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
	parms.keepalive_time = htons(leconf->keepalive);
	parms.reserved = 0;
	parms.pvlim = 0;
	parms.max_pdu_len = 0;
	parms.lsr_id = nbr->id.s_addr;
	parms.lspace_id = 0;

	return (ibuf_add(buf, &parms, SESS_PRMS_SIZE));
}

int
tlv_decode_opt_init_prms(char *buf, u_int16_t len)
{
	struct tlv	tlv;
	int		cons = 0;
	u_int16_t	tlv_len;

	 while (len >= sizeof(tlv)) {
		bcopy(buf, &tlv, sizeof(tlv));
		tlv_len = ntohs(tlv.length);
		switch (ntohs(tlv.type)) {
		case TLV_TYPE_ATMSESSIONPAR:
			log_warnx("ATM session parameter present");
			return (-1);
		case TLV_TYPE_FRSESSION:
			log_warnx("FR session parameter present");
			return (-1);
		default:
			/* if unknown flag set, ignore TLV */
			if (!(ntohs(tlv.type) & UNKNOWN_FLAG))
				return (-1);
			break;
		}
		buf += TLV_HDR_LEN + tlv_len;
		len -= TLV_HDR_LEN + tlv_len;
		cons += TLV_HDR_LEN + tlv_len;
	}

	return (cons);
}

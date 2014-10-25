/*	$OpenBSD: notification.c,v 1.17 2014/10/25 03:23:49 lteo Exp $ */

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

int	gen_status_tlv(struct ibuf *, u_int32_t, u_int32_t, u_int32_t);

void
send_notification_nbr(struct nbr *nbr, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	log_debug("send_notification_nbr: nbr ID %s, status %s",
	    inet_ntoa(nbr->id), notification_name(status));
	send_notification(status, nbr->tcp, msgid, type);
	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

void
send_notification(u_int32_t status, struct tcp_conn *tcp, u_int32_t msgid,
    u_int32_t type)
{
	struct ibuf	*buf;
	u_int16_t	 size;

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_notification");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) + STATUS_SIZE;

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_NOTIFICATION, size);

	size -= sizeof(struct ldp_msg);

	gen_status_tlv(buf, status, msgid, type);

	evbuf_enqueue(&tcp->wbuf, buf);
}

int
recv_notification(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		not;
	struct status_tlv	st;

	log_debug("recv_notification: neighbor ID %s", inet_ntoa(nbr->id));

	bcopy(buf, &not, sizeof(not));

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	if (len < STATUS_SIZE) {
		session_shutdown(nbr, S_BAD_MSG_LEN, not.msgid, not.type);
		return (-1);
	}
	bcopy(buf, &st, sizeof(st));

	if (ntohs(st.length) > STATUS_SIZE - TLV_HDR_LEN ||
	    ntohs(st.length) > len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, not.msgid, not.type);
		return (-1);
	}

	/* TODO optional parameters: ext status, returned PDU and msg */

	if (st.status_code & htonl(STATUS_FATAL))
		log_warnx("received notification from neighbor %s: %s",
		    inet_ntoa(nbr->id),
		    notification_name(ntohl(st.status_code)));
	else
		log_debug("received non-fatal notification from neighbor "
		    "%s: %s", inet_ntoa(nbr->id),
		    notification_name(ntohl(st.status_code)));

	if (st.status_code & htonl(STATUS_FATAL)) {
		if (st.status_code == htonl(S_NO_HELLO) ||
		    st.status_code == htonl(S_PARM_ADV_MODE) ||
		    st.status_code == htonl(S_MAX_PDU_LEN) ||
		    st.status_code == htonl(S_PARM_L_RANGE) ||
		    st.status_code == htonl(S_KEEPALIVE_BAD))
			nbr_start_idtimer(nbr);

		nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		return (-1);
	}
	/* XXX in some cases we should inform the RDE about non-fatal ones */

	return (ntohs(not.length));
}

int
gen_status_tlv(struct ibuf *buf, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	struct status_tlv	st;

	bzero(&st, sizeof(st));

	st.type = htons(TLV_TYPE_STATUS);
	st.length = htons(STATUS_TLV_LEN);
	st.status_code = htonl(status);

	st.msg_id = msgid;
	st.msg_type = type;

	return (ibuf_add(buf, &st, STATUS_SIZE));
}

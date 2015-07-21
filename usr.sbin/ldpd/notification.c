/*	$OpenBSD: notification.c,v 1.18 2015/07/21 04:52:29 renato Exp $ */

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
send_notification_full(struct tcp_conn *tcp, struct notify_msg *nm)
{
	struct ibuf	*buf;
	u_int16_t	 size;

	if (tcp->nbr)
		log_debug("send_notification_full: nbr ID %s, status %s",
		    inet_ntoa(tcp->nbr->id), notification_name(nm->status));

	if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_notification");

	/* calculate size */
	size = LDP_HDR_SIZE + sizeof(struct ldp_msg) + STATUS_SIZE;
	if (nm->flags & F_NOTIF_PW_STATUS)
		size += PW_STATUS_TLV_LEN;
	if (nm->flags & F_NOTIF_FEC) {
		size += TLV_HDR_LEN;
		switch (nm->fec.type) {
		case FEC_PWID:
			size += FEC_PWID_ELM_MIN_LEN;
			if (nm->fec.flags & F_MAP_PW_ID)
				size += sizeof(u_int32_t);
			break;
		}
	}

	gen_ldp_hdr(buf, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_NOTIFICATION, size);

	gen_status_tlv(buf, nm->status, nm->messageid, nm->type);
	/* optional tlvs */
	if (nm->flags & F_NOTIF_PW_STATUS)
		gen_pw_status_tlv(buf, nm->pw_status);
	if (nm->flags & F_NOTIF_FEC)
		gen_fec_tlv(buf, &nm->fec);

	evbuf_enqueue(&tcp->wbuf, buf);
}

/* send a notification without optional tlvs */
void
send_notification(u_int32_t status, struct tcp_conn *tcp, u_int32_t msgid,
    u_int32_t type)
{
	struct notify_msg	 nm;

	bzero(&nm, sizeof(nm));
	nm.status = status;
	nm.messageid = msgid;
	nm.type = type;

	send_notification_full(tcp, &nm);
}

void
send_notification_nbr(struct nbr *nbr, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	send_notification(status, nbr->tcp, msgid, type);
	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

int
recv_notification(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		not;
	struct status_tlv	st;
	struct notify_msg	nm;
	int			tlen;

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
	buf += STATUS_SIZE;
	len -= STATUS_SIZE;

	bzero(&nm, sizeof(nm));
	nm.status = ntohl(st.status_code);

	/* Optional Parameters */
	while (len > 0) {
		struct tlv 	tlv;

		if (len < sizeof(tlv)) {
			session_shutdown(nbr, S_BAD_TLV_LEN, not.msgid,
			    not.type);
			return (-1);
		}

		bcopy(buf, &tlv, sizeof(tlv));
		if (ntohs(tlv.length) > len - TLV_HDR_LEN) {
			session_shutdown(nbr, S_BAD_TLV_LEN, not.msgid,
			    not.type);
			return (-1);
		}
		buf += TLV_HDR_LEN;
		len -= TLV_HDR_LEN;

		switch (ntohs(tlv.type) & ~UNKNOWN_FLAG) {
		case TLV_TYPE_EXTSTATUS:
		case TLV_TYPE_RETURNEDPDU:
		case TLV_TYPE_RETURNEDMSG:
			/* TODO is there any use for this? */
			break;
		case TLV_TYPE_PW_STATUS:
			if (ntohs(tlv.length) != 4) {
				session_shutdown(nbr, S_BAD_TLV_LEN,
				    not.msgid, not.type);
				return (-1);
			}

			nm.pw_status = ntohl(*(u_int32_t *)buf);
			nm.flags |= F_NOTIF_PW_STATUS;
			break;
		case TLV_TYPE_FEC:
			if ((tlen = tlv_decode_fec_elm(nbr, &not, buf,
			    ntohs(tlv.length), &nm.fec)) == -1)
				return (-1);
			/* allow only one fec element */
			if (tlen != ntohs(tlv.length)) {
				session_shutdown(nbr, S_BAD_TLV_VAL,
				    not.msgid, not.type);
				return (-1);
			}
			nm.flags |= F_NOTIF_FEC;
			break;
		default:
			if (!(ntohs(tlv.type) & UNKNOWN_FLAG)) {
				send_notification_nbr(nbr, S_UNKNOWN_TLV,
				    not.msgid, not.type);
			}
			/* ignore unknown tlv */
			break;
		}
		buf += ntohs(tlv.length);
		len -= ntohs(tlv.length);
	}

	if (nm.status == S_PW_STATUS) {
		if (!(nm.flags & (F_NOTIF_PW_STATUS|F_NOTIF_FEC))) {
			send_notification_nbr(nbr, S_MISS_MSG,
			    not.msgid, not.type);
			return (-1);
		}

		switch (nm.fec.type) {
		case FEC_PWID:
		case FEC_GENPWID:
			break;
		default:
			send_notification_nbr(nbr, S_BAD_TLV_VAL,
			    not.msgid, not.type);
			return (-1);
			break;
		}
	}

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

	if (nm.status == S_PW_STATUS)
		ldpe_imsg_compose_lde(IMSG_NOTIFICATION, nbr->peerid, 0,
		    &nm, sizeof(nm));

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

	st.msg_id = htonl(msgid);
	st.msg_type = htonl(type);

	return (ibuf_add(buf, &st, STATUS_SIZE));
}

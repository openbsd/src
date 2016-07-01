/*	$OpenBSD: notification.c,v 1.37 2016/07/01 23:29:55 renato Exp $ */

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
#include <arpa/inet.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

static int	 gen_status_tlv(struct ibuf *, uint32_t, uint32_t, uint16_t);

void
send_notification_full(struct tcp_conn *tcp, struct notify_msg *nm)
{
	struct ibuf	*buf;
	uint16_t	 size;
	int		 err = 0;

	/* calculate size */
	size = LDP_HDR_SIZE + LDP_MSG_SIZE + STATUS_SIZE;
	if (nm->flags & F_NOTIF_PW_STATUS)
		size += PW_STATUS_TLV_LEN;
	if (nm->flags & F_NOTIF_FEC) {
		size += TLV_HDR_LEN;
		switch (nm->fec.type) {
		case MAP_TYPE_PWID:
			size += FEC_PWID_ELM_MIN_LEN;
			if (nm->fec.flags & F_MAP_PW_ID)
				size += sizeof(uint32_t);
			break;
		}
	}

	if ((buf = ibuf_open(size)) == NULL)
		fatal(__func__);

	err |= gen_ldp_hdr(buf, size);
	size -= LDP_HDR_SIZE;
	err |= gen_msg_hdr(buf, MSG_TYPE_NOTIFICATION, size);
	err |= gen_status_tlv(buf, nm->status, nm->messageid, nm->type);
	/* optional tlvs */
	if (nm->flags & F_NOTIF_PW_STATUS)
		err |= gen_pw_status_tlv(buf, nm->pw_status);
	if (nm->flags & F_NOTIF_FEC)
		err |= gen_fec_tlv(buf, &nm->fec);
	if (err) {
		ibuf_free(buf);
		return;
	}

	evbuf_enqueue(&tcp->wbuf, buf);
}

/* send a notification without optional tlvs */
void
send_notification(uint32_t status, struct tcp_conn *tcp, uint32_t msgid,
    uint16_t type)
{
	struct notify_msg	 nm;

	memset(&nm, 0, sizeof(nm));
	nm.status = status;
	nm.messageid = msgid;
	nm.type = type;

	send_notification_full(tcp, &nm);
}

void
send_notification_nbr(struct nbr *nbr, uint32_t status, uint32_t msgid,
    uint16_t type)
{
	log_debug("%s: lsr-id %s, status %s", __func__, inet_ntoa(nbr->id),
	     notification_name(status));

	send_notification(status, nbr->tcp, msgid, type);
	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

int
recv_notification(struct nbr *nbr, char *buf, uint16_t len)
{
	struct ldp_msg		not;
	struct status_tlv	st;
	struct notify_msg	nm;
	int			tlen;

	memcpy(&not, buf, sizeof(not));
	buf += LDP_MSG_SIZE;
	len -= LDP_MSG_SIZE;

	if (len < STATUS_SIZE) {
		session_shutdown(nbr, S_BAD_MSG_LEN, not.msgid, not.type);
		return (-1);
	}
	memcpy(&st, buf, sizeof(st));

	if (ntohs(st.length) > STATUS_SIZE - TLV_HDR_LEN ||
	    ntohs(st.length) > len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, not.msgid, not.type);
		return (-1);
	}
	buf += STATUS_SIZE;
	len -= STATUS_SIZE;

	memset(&nm, 0, sizeof(nm));
	nm.status = ntohl(st.status_code);

	/* Optional Parameters */
	while (len > 0) {
		struct tlv 	tlv;
		uint16_t	tlv_len;

		if (len < sizeof(tlv)) {
			session_shutdown(nbr, S_BAD_TLV_LEN, not.msgid,
			    not.type);
			return (-1);
		}

		memcpy(&tlv, buf, TLV_HDR_LEN);
		buf += TLV_HDR_LEN;
		len -= TLV_HDR_LEN;
		tlv_len = ntohs(tlv.length);

		switch (ntohs(tlv.type)) {
		case TLV_TYPE_EXTSTATUS:
		case TLV_TYPE_RETURNEDPDU:
		case TLV_TYPE_RETURNEDMSG:
			/* TODO is there any use for this? */
			break;
		case TLV_TYPE_PW_STATUS:
			if (tlv_len != 4) {
				session_shutdown(nbr, S_BAD_TLV_LEN,
				    not.msgid, not.type);
				return (-1);
			}

			nm.pw_status = ntohl(*(uint32_t *)buf);
			nm.flags |= F_NOTIF_PW_STATUS;
			break;
		case TLV_TYPE_FEC:
			if ((tlen = tlv_decode_fec_elm(nbr, &not, buf,
			    tlv_len, &nm.fec)) == -1)
				return (-1);
			/* allow only one fec element */
			if (tlen != tlv_len) {
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
		buf += tlv_len;
		len -= tlv_len;
	}

	if (nm.status == S_PW_STATUS) {
		if (!(nm.flags & (F_NOTIF_PW_STATUS|F_NOTIF_FEC))) {
			send_notification_nbr(nbr, S_MISS_MSG,
			    not.msgid, not.type);
			return (-1);
		}

		switch (nm.fec.type) {
		case MAP_TYPE_PWID:
			break;
		default:
			send_notification_nbr(nbr, S_BAD_TLV_VAL,
			    not.msgid, not.type);
			return (-1);
		}
	}

	if (st.status_code & htonl(STATUS_FATAL))
		log_warnx("received notification from lsr-id %s: %s",
		    inet_ntoa(nbr->id),
		    notification_name(ntohl(st.status_code)));
	else
		log_debug("received non-fatal notification from lsr-id "
		    "%s: %s", inet_ntoa(nbr->id),
		    notification_name(ntohl(st.status_code)));

	if (st.status_code & htonl(STATUS_FATAL)) {
		if (nbr->state == NBR_STA_OPENSENT)
			nbr_start_idtimer(nbr);

		nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		return (-1);
	}

	if (nm.status == S_PW_STATUS)
		ldpe_imsg_compose_lde(IMSG_NOTIFICATION, nbr->peerid, 0,
		    &nm, sizeof(nm));

	return (0);
}

static int
gen_status_tlv(struct ibuf *buf, uint32_t status, uint32_t msgid, uint16_t type)
{
	struct status_tlv	st;

	memset(&st, 0, sizeof(st));

	st.type = htons(TLV_TYPE_STATUS);
	st.length = htons(STATUS_TLV_LEN);
	st.status_code = htonl(status);

	/* for convenience, msgid and type are already in network byte order */
	st.msg_id = msgid;
	st.msg_type = type;

	return (ibuf_add(buf, &st, STATUS_SIZE));
}

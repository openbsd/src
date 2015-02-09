/*	$OpenBSD: labelmapping.c,v 1.31 2015/02/09 11:53:25 claudio Exp $ */

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
#include <netmpls/mpls.h>
#include <unistd.h>

#include <errno.h>
#include <event.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

void		gen_label_tlv(struct ibuf *, u_int32_t);
void		gen_reqid_tlv(struct ibuf *, u_int32_t);
void		gen_fec_tlv(struct ibuf *, struct in_addr, u_int8_t);

int	tlv_decode_label(struct nbr *, struct ldp_msg *, char *, u_int16_t,
    u_int32_t *);
int	tlv_decode_fec_elm(struct nbr *, struct ldp_msg *, char *, u_int16_t,
    u_int8_t *, u_int32_t *, u_int8_t *);

static void
enqueue_pdu(struct nbr *nbr, struct ibuf *buf, u_int16_t size)
{
	struct ldp_hdr		*ldp_hdr;

	ldp_hdr = ibuf_seek(buf, 0, sizeof(struct ldp_hdr));
	ldp_hdr->length = htons(size);
	evbuf_enqueue(&nbr->tcp->wbuf, buf);
}

/* Generic function that handles all Label Message types */
void
send_labelmessage(struct nbr *nbr, u_int16_t type, struct mapping_head *mh)
{
	struct ibuf		*buf = NULL;
	struct mapping_entry	*me;
	u_int16_t		 tlv_size, size = 0;
	int			 first = 1;

	while ((me = TAILQ_FIRST(mh)) != NULL) {
		/* generate pdu */
		if (first) {
			if ((buf = ibuf_open(LDP_MAX_LEN)) == NULL)
				fatal("send_labelmapping");

			/* real size will be set up later */
			gen_ldp_hdr(buf, 0);

			size = LDP_HDR_PDU_LEN;
			first = 0;
		}

		/* calculate size */
		tlv_size = LDP_MSG_LEN + TLV_HDR_LEN + FEC_ELM_MIN_LEN +
		    PREFIX_SIZE(me->map.prefixlen);
		if (type == MSG_TYPE_LABELMAPPING ||
		    me->map.flags & F_MAP_OPTLABEL)
			tlv_size += LABEL_TLV_LEN;
		if (me->map.flags & F_MAP_REQ_ID)
			tlv_size += REQID_TLV_LEN;

		/* maximum pdu length exceeded, we need a new ldp pdu */
		if (size + tlv_size > LDP_MAX_LEN) {
			enqueue_pdu(nbr, buf, size);
			first = 1;
			continue;
		}

		size += tlv_size;

		/* append message and tlvs */
		gen_msg_tlv(buf, type, tlv_size);
		gen_fec_tlv(buf, me->map.prefix, me->map.prefixlen);
		if (type == MSG_TYPE_LABELMAPPING ||
		    me->map.flags & F_MAP_OPTLABEL)
			gen_label_tlv(buf, me->map.label);
		if (me->map.flags & F_MAP_REQ_ID)
			gen_reqid_tlv(buf, me->map.requestid);

		TAILQ_REMOVE(mh, me, entry);
		free(me);
	}

	enqueue_pdu(nbr, buf, size);

	nbr_fsm(nbr, NBR_EVT_PDU_SENT);
}

/* Generic function that handles all Label Message types */
int
recv_labelmessage(struct nbr *nbr, char *buf, u_int16_t len, u_int16_t type)
{
	struct ldp_msg		 	 lm;
	struct tlv			 ft;
	u_int32_t			 label, reqid = 0;
	u_int8_t			 flags = 0;

	int				 feclen, lbllen, tlen;
	u_int8_t			 addr_type;
	struct mapping_entry		*me;
	struct mapping_head		 mh;

	bcopy(buf, &lm, sizeof(lm));

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	/* FEC TLV */
	if (len < sizeof(ft)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm.msgid, lm.type);
		return (-1);
	}

	bcopy(buf, &ft, sizeof(ft));
	if (ntohs(ft.type) != TLV_TYPE_FEC) {
		send_notification_nbr(nbr, S_MISS_MSG, lm.msgid, lm.type);
		return (-1);
	}
	feclen = ntohs(ft.length);

	if (feclen > len - TLV_HDR_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm.msgid, lm.type);
		return (-1);
	}

	buf += TLV_HDR_LEN;	/* just advance to the end of the fec header */
	len -= TLV_HDR_LEN;

	TAILQ_INIT(&mh);
	do {
		me = calloc(1, sizeof(*me));
		me->map.messageid = lm.msgid;
		TAILQ_INSERT_HEAD(&mh, me, entry);

		if ((tlen = tlv_decode_fec_elm(nbr, &lm, buf, feclen,
		    &addr_type, &me->map.prefix.s_addr, &me->map.prefixlen)) == -1)
			goto err;

		/*
		 * The Wildcard FEC Element can be used only in the
		 * Label Withdraw and Label Release messages.
		 */
		if (addr_type == FEC_WILDCARD) {
			switch (type) {
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
				me->map.flags |= F_MAP_WILDCARD;
				break;
			default:
				session_shutdown(nbr, S_BAD_TLV_VAL, lm.msgid,
				    lm.type);
				goto err;
				break;
			}
		}

		/*
		 * LDP supports the use of multiple FEC Elements per
		 * FEC for the Label Mapping message only.
		 */
		if (type != MSG_TYPE_LABELMAPPING &&
		    tlen != feclen) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lm.msgid,
			    lm.type);
			goto err;
		}

		buf += tlen;
		len -= tlen;
		feclen -= tlen;
	} while (feclen > 0);

	/* Mandatory Label TLV */
	if (type == MSG_TYPE_LABELMAPPING) {
		lbllen = tlv_decode_label(nbr, &lm, buf, len, &label);
		if (lbllen == -1)
			goto err;

		buf += lbllen;
		len -= lbllen;
	}

	/* Optional Parameters */
	while (len > 0) {
		struct tlv 	tlv;
		u_int32_t reqbuf, labelbuf;

		if (len < sizeof(tlv)) {
			session_shutdown(nbr, S_BAD_TLV_LEN, lm.msgid,
			    lm.type);
			goto err;
		}

		bcopy(buf, &tlv, sizeof(tlv));
		buf += TLV_HDR_LEN;
		len -= TLV_HDR_LEN;

		switch (ntohs(tlv.type)) {
		case TLV_TYPE_LABELREQUEST:
			switch (type) {
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELABORTREQ:
				if (ntohs(tlv.length) != 4) {
					session_shutdown(nbr, S_BAD_TLV_LEN,
					    lm.msgid, lm.type);
					goto err;
				}

				flags |= F_MAP_REQ_ID;
				memcpy(&reqbuf, buf, sizeof(reqbuf));
				reqid = ntohl(reqbuf);
				break;
			default:
				/* ignore */
				break;
			}
			break;
		case TLV_TYPE_HOPCOUNT:
		case TLV_TYPE_PATHVECTOR:
			/* TODO just ignore for now */
			break;
		case TLV_TYPE_GENERICLABEL:
			switch (type) {
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
				if (ntohs(tlv.length) != 4) {
					session_shutdown(nbr, S_BAD_TLV_LEN,
					    lm.msgid, lm.type);
					goto err;
				}

				memcpy(&labelbuf, buf, sizeof(labelbuf));
				label = ntohl(labelbuf);
				flags |= F_MAP_OPTLABEL;
				break;
			default:
				/* ignore */
				break;
			}
			break;
		case TLV_TYPE_ATMLABEL:
		case TLV_TYPE_FRLABEL:
			switch (type) {
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
				/* unsupported */
				session_shutdown(nbr, S_BAD_TLV_VAL, lm.msgid,
				    lm.type);
				goto err;
				break;
			default:
				/* ignore */
				break;
			}
			break;
		default:
			if (!(ntohs(tlv.type) & UNKNOWN_FLAG)) {
				send_notification_nbr(nbr, S_UNKNOWN_TLV,
				    lm.msgid, lm.type);
			}
			/* ignore unknown tlv */
			break;
		}
		buf += ntohs(tlv.length);
		len -= ntohs(tlv.length);
	}

	/* notify lde about the received message. */
	while ((me = TAILQ_FIRST(&mh)) != NULL) {
		int imsg_type = IMSG_NONE;

		me->map.flags |= flags;
		if (type == MSG_TYPE_LABELMAPPING ||
		    me->map.flags & F_MAP_OPTLABEL)
			me->map.label = label;
		if (me->map.flags & F_MAP_REQ_ID)
			me->map.requestid = reqid;

		switch (type) {
		case MSG_TYPE_LABELMAPPING:
			imsg_type = IMSG_LABEL_MAPPING;
			break;
		case MSG_TYPE_LABELREQUEST:
			imsg_type = IMSG_LABEL_REQUEST;
			break;
		case MSG_TYPE_LABELWITHDRAW:
			imsg_type = IMSG_LABEL_WITHDRAW;
			break;
		case MSG_TYPE_LABELRELEASE:
			imsg_type = IMSG_LABEL_RELEASE;
			break;
		case MSG_TYPE_LABELABORTREQ:
			imsg_type = IMSG_LABEL_ABORT;
			break;
		default:
			break;
		}

		ldpe_imsg_compose_lde(imsg_type, nbr->peerid, 0, &me->map,
		    sizeof(struct map));

		TAILQ_REMOVE(&mh, me, entry);
		free(me);
	}

	return (ntohs(lm.length));

err:
	mapping_list_clr(&mh);

	return (-1);
}

/* Other TLV related functions */
void
gen_label_tlv(struct ibuf *buf, u_int32_t label)
{
	struct label_tlv	lt;

	lt.type = htons(TLV_TYPE_GENERICLABEL);
	lt.length = htons(sizeof(label));
	lt.label = htonl(label);

	ibuf_add(buf, &lt, sizeof(lt));
}

int
tlv_decode_label(struct nbr *nbr, struct ldp_msg *lm, char *buf,
    u_int16_t len, u_int32_t *label)
{
	struct label_tlv lt;

	if (len < sizeof(lt)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid, lm->type);
		return (-1);
	}
	bcopy(buf, &lt, sizeof(lt));

	if (!(ntohs(lt.type) & TLV_TYPE_GENERICLABEL)) {
		send_notification_nbr(nbr, S_MISS_MSG, lm->msgid, lm->type);
		return (-1);
	}

	switch (htons(lt.type)) {
	case TLV_TYPE_GENERICLABEL:
		if (ntohs(lt.length) != sizeof(lt) - TLV_HDR_LEN) {
			session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid,
			    lm->type);
			return (-1);
		}

		*label = ntohl(lt.label);
		if (*label > MPLS_LABEL_MAX ||
		    (*label <= MPLS_LABEL_RESERVED_MAX &&
		     *label != MPLS_LABEL_IPV4NULL &&
		     *label != MPLS_LABEL_IMPLNULL)) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lm->msgid,
			    lm->type);
			return (-1);
		}
		break;
	case TLV_TYPE_ATMLABEL:
	case TLV_TYPE_FRLABEL:
	default:
		/* unsupported */
		session_shutdown(nbr, S_BAD_TLV_VAL, lm->msgid, lm->type);
		return (-1);
	}

	return (sizeof(lt));
}

void
gen_reqid_tlv(struct ibuf *buf, u_int32_t reqid)
{
	struct reqid_tlv	rt;

	rt.type = htons(TLV_TYPE_LABELREQUEST);
	rt.length = htons(sizeof(reqid));
	rt.reqid = htonl(reqid);

	ibuf_add(buf, &rt, sizeof(rt));
}

void
gen_fec_tlv(struct ibuf *buf, struct in_addr prefix, u_int8_t prefixlen)
{
	struct tlv	ft;
	u_int8_t	type;
	u_int16_t	family;
	u_int8_t	len;

	len = PREFIX_SIZE(prefixlen);
	ft.type = htons(TLV_TYPE_FEC);
	ft.length = htons(sizeof(type) + sizeof(family) + sizeof(prefixlen) +
	    len);

	ibuf_add(buf, &ft, sizeof(ft));

	type = FEC_PREFIX;
	family = htons(FEC_IPV4);

	ibuf_add(buf, &type, sizeof(type));
	ibuf_add(buf, &family, sizeof(family));
	ibuf_add(buf, &prefixlen, sizeof(prefixlen));
	if (len)
		ibuf_add(buf, &prefix, len);
}

int
tlv_decode_fec_elm(struct nbr *nbr, struct ldp_msg *lm, char *buf,
    u_int16_t len, u_int8_t *type, u_int32_t *prefix, u_int8_t *prefixlen)
{
	u_int16_t	family, off = 0;

	*type = *buf;
	off += sizeof(u_int8_t);

	if (*type == FEC_WILDCARD) {
		if (len == 0)
			return (off);
		else {
			session_shutdown(nbr, S_BAD_TLV_VAL, lm->msgid,
			    lm->type);
			return (-1);
		}
	}

	if (*type != FEC_PREFIX) {
		send_notification_nbr(nbr, S_UNKNOWN_FEC, lm->msgid, lm->type);
		return (-1);
	}

	if (len < FEC_ELM_MIN_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid, lm->type);
		return (-1);
	}

	bcopy(buf + off, &family, sizeof(family));
	off += sizeof(family);

	if (family != htons(FEC_IPV4)) {
		send_notification_nbr(nbr, S_UNSUP_ADDR, lm->msgid, lm->type);
		return (-1);
	}

	*prefixlen = buf[off];
	off += sizeof(u_int8_t);

	if (len < off + PREFIX_SIZE(*prefixlen)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid, lm->type);
		return (-1);
	}

	*prefix = 0;
	bcopy(buf + off, prefix, PREFIX_SIZE(*prefixlen));

	return (off + PREFIX_SIZE(*prefixlen));
}

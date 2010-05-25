/*	$OpenBSD: labelmapping.c,v 1.10 2010/05/25 09:40:10 claudio Exp $ */

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

void		gen_fec_tlv(struct buf *, u_int32_t, u_int8_t);
void		gen_label_tlv(struct buf *, u_int32_t);

u_int32_t	tlv_decode_label(struct label_tlv *);
int		tlv_decode_fec_elm(char *, u_int16_t, u_int8_t *, u_int32_t *,
		    u_int8_t *);

/* Label Mapping Message */
void
send_labelmapping(struct nbr *nbr)
{
	struct buf		*buf;
	struct mapping_entry	*me;
	struct ldp_hdr		*ldp_hdr;
	u_int16_t		 tlv_size, size;

	if (nbr->iface->passive)
		return;

	log_debug("send_labelmapping: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_labelmapping");

	/* real size will be set up later */
	gen_ldp_hdr(buf, nbr->iface, 0);

	size = LDP_HDR_SIZE - TLV_HDR_LEN;

	TAILQ_FOREACH(me, &nbr->mapping_list, entry) {
		tlv_size = BASIC_LABEL_MAP_LEN + PREFIX_SIZE(me->prefixlen);
		size += tlv_size;

		gen_msg_tlv(buf, MSG_TYPE_LABELMAPPING, tlv_size);
		gen_fec_tlv(buf, me->prefix, me->prefixlen);
		gen_label_tlv(buf, me->label);
	}

	/* XXX: should we remove them first? */
	nbr_mapping_list_clr(nbr, &nbr->mapping_list);

	ldp_hdr = buf_seek(buf, 0, sizeof(struct ldp_hdr));
	ldp_hdr->length = htons(size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_labelmapping(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg		*lm;
	struct fec_tlv		*ft;
	struct label_tlv	*lt;
	struct map		 map;
	int			 feclen, tlen;
	u_int8_t		 addr_type;

	log_debug("recv_labelmapping: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->state != NBR_STA_OPER)
		return (-1);

	lm = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(lm->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, lm->msgid, lm->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	ft = (struct fec_tlv *)buf;

	if (len < sizeof(*ft) ||
	    (len - TLV_HDR_LEN) < ntohs(ft->length)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid, lm->type);
		return (-1);
	}

	feclen = ntohs(ft->length);
	buf += sizeof(struct fec_tlv);
	len -= sizeof(struct fec_tlv);

	if (len < feclen + LABEL_TLV_LEN) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lm->msgid, lm->type);
		return (-1);
	}

	bzero(&map, sizeof(map));
	map.messageid = lm->msgid;

	lt = (struct label_tlv *)(buf + feclen);
	map.label = tlv_decode_label(lt);
	if (map.label == NO_LABEL) {
		session_shutdown(nbr, S_BAD_TLV_VAL, lm->msgid, lm->type);
		return (-1);
	}

	do {
		if ((tlen = tlv_decode_fec_elm(buf, feclen, &addr_type,
		    &map.prefix, &map.prefixlen)) == -1 ||
		    addr_type == FEC_WILDCARD) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lm->msgid,
			    lm->type);
			return (-1);
		}

		ldpe_imsg_compose_lde(IMSG_LABEL_MAPPING, nbr->peerid, 0, &map,
		    sizeof(map));

		buf += tlen;
		feclen -= tlen;
	} while (feclen > 0);

	nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(lm->length));
}

/* Label Request Message */
void
send_labelrequest(struct nbr *nbr)
{
	struct buf		*buf;
	struct mapping_entry	*me;
	struct ldp_hdr		*ldp_hdr;
	u_int16_t		 tlv_size, size;

	if (nbr->iface->passive)
		return;

	log_debug("send_labelrequest: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_labelrequest");

	/* real size will be set up later */
	gen_ldp_hdr(buf, nbr->iface, 0);

	size = LDP_HDR_SIZE - TLV_HDR_LEN;

	TAILQ_FOREACH(me, &nbr->request_list, entry) {
		tlv_size = PREFIX_SIZE(me->prefixlen);
		size += tlv_size;

		gen_msg_tlv(buf, MSG_TYPE_LABELREQUEST, tlv_size);
		gen_fec_tlv(buf, me->prefix, me->prefixlen);
	}

	/* XXX: should we remove them first? */
	nbr_mapping_list_clr(nbr, &nbr->request_list);

	ldp_hdr = buf_seek(buf, 0, sizeof(struct ldp_hdr));
	ldp_hdr->length = htons(size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_labelrequest(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg	*lr;
	struct fec_tlv	*ft;
	struct map	 map;
	int		 feclen, tlen;
	u_int8_t	 addr_type;

	log_debug("recv_labelrequest: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->state != NBR_STA_OPER)
		return (-1);

	lr = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(lr->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, lr->msgid, lr->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	ft = (struct fec_tlv *)buf;

	if (len < sizeof(*ft) ||
	    (len - TLV_HDR_LEN) < ntohs(ft->length)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lr->msgid, lr->type);
		return (-1);
	}

	feclen = ntohs(ft->length);
	buf += sizeof(struct fec_tlv);
	len -= sizeof(struct fec_tlv);

	bzero(&map, sizeof(map));
	map.messageid = lr->msgid;

	do {
		if ((tlen = tlv_decode_fec_elm(buf, feclen, &addr_type,
		    &map.prefix, &map.prefixlen)) == -1 ||
		    addr_type == FEC_WILDCARD) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lr->msgid,
			    lr->type);
			return (-1);
		}

		ldpe_imsg_compose_lde(IMSG_LABEL_REQUEST, nbr->peerid, 0, &map,
		    sizeof(map));

		buf += tlen;
		feclen -= tlen;
	} while (feclen > 0);

	nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(lr->length));
}

/* Label Withdraw Message */
void
send_labelwithdraw(struct nbr *nbr)
{
	struct buf		*buf;
	struct mapping_entry	*me;
	struct ldp_hdr		*ldp_hdr;
	u_int16_t		 tlv_size, size;

	if (nbr->iface->passive)
		return;

	log_debug("send_labelwithdraw: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_labelwithdraw");

	/* real size will be set up later */
	gen_ldp_hdr(buf, nbr->iface, 0);

	size = LDP_HDR_SIZE - TLV_HDR_LEN;

	TAILQ_FOREACH(me, &nbr->withdraw_list, entry) {
		if (me->label == NO_LABEL)
			tlv_size = PREFIX_SIZE(me->prefixlen);
		else
			tlv_size = BASIC_LABEL_MAP_LEN +
			    PREFIX_SIZE(me->prefixlen);

		size += tlv_size;

		gen_msg_tlv(buf, MSG_TYPE_LABELWITHDRAW, tlv_size);
		gen_fec_tlv(buf, me->prefix, me->prefixlen);

		if (me->label != NO_LABEL)
			gen_label_tlv(buf, me->label);
	}

	/* XXX: should we remove them first? */
	nbr_mapping_list_clr(nbr, &nbr->withdraw_list);

	ldp_hdr = buf_seek(buf, 0, sizeof(struct ldp_hdr));
	ldp_hdr->length = htons(size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_labelwithdraw(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct map	 map;
	struct ldp_msg	*lw;
	struct fec_tlv	*ft;
	u_int32_t	 optlabel = NO_LABEL;
	int		 feclen, tlen;
	u_int8_t	 addr_type;

	log_debug("recv_labelwithdraw: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->state != NBR_STA_OPER)
		return (-1);

	lw = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(lw->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, lw->msgid, lw->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	ft = (struct fec_tlv *)buf;

	if (len < sizeof(*ft) ||
	    (len - TLV_HDR_LEN) < ntohs(ft->length)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lw->msgid, lw->type);
		return (-1);
	}

	feclen = ntohs(ft->length);
	buf += sizeof(struct fec_tlv);
	len -= sizeof(struct fec_tlv);

	/* release may include optional label */
	if (len >= feclen) {
		struct label_tlv	*lt;

		lt = (struct label_tlv *)(buf + feclen);
		optlabel = tlv_decode_label(lt);
		if (optlabel == NO_LABEL) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lw->msgid,
			    lw->type);
			return (-1);
		}
	}

	bzero(&map, sizeof(map));
	map.messageid = lw->msgid;
	if (optlabel != NO_LABEL) {
		map.label = optlabel;
		map.flags = F_MAP_OPTLABEL;
	}
	do {
		if ((tlen = tlv_decode_fec_elm(buf, feclen, &addr_type,
		    &map.prefix, &map.prefixlen)) == -1) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lw->msgid,
			    lw->type);
			return (-1);
		}

		if (addr_type == FEC_WILDCARD) {
			map.prefix = 0;
			map.prefixlen = 0;
			map.flags |= F_MAP_WILDCARD;

		} else
			map.flags &= ~F_MAP_WILDCARD;

		ldpe_imsg_compose_lde(IMSG_LABEL_WITHDRAW, nbr->peerid, 0, &map,
		    sizeof(map));

		buf += tlen;
		feclen -= tlen;
	} while (feclen > 0);

	nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(lw->length));
}

/* Label Release Message */
void
send_labelrelease(struct nbr *nbr)
{
	struct buf		*buf;
	struct mapping_entry	*me;
	struct ldp_hdr		*ldp_hdr;
	u_int16_t		 tlv_size, size;

	if (nbr->iface->passive)
		return;

	log_debug("send_labelrelease: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_labelrelease");

	/* real size will be set up later */
	gen_ldp_hdr(buf, nbr->iface, 0);

	size = LDP_HDR_SIZE - TLV_HDR_LEN;

	TAILQ_FOREACH(me, &nbr->release_list, entry) {
		if (me->label == NO_LABEL)
			tlv_size = PREFIX_SIZE(me->prefixlen);
		else
			tlv_size = BASIC_LABEL_MAP_LEN +
			    PREFIX_SIZE(me->prefixlen);

		size += tlv_size;

		gen_msg_tlv(buf, MSG_TYPE_LABELRELEASE, tlv_size);
		gen_fec_tlv(buf, me->prefix, me->prefixlen);

		if (me->label != NO_LABEL)
			gen_label_tlv(buf, me->label);
	}

	/* XXX: should we remove them first? */
	nbr_mapping_list_clr(nbr, &nbr->release_list);

	ldp_hdr = buf_seek(buf, 0, sizeof(struct ldp_hdr));
	ldp_hdr->length = htons(size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_labelrelease(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct map	 map;
	struct ldp_msg	*lr;
	struct fec_tlv	*ft;
	u_int32_t	 optlabel = NO_LABEL;
	int		 feclen, tlen;
	u_int8_t	 addr_type;

	log_debug("recv_labelrelease: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->state != NBR_STA_OPER)
		return (-1);

	lr = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(lr->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, lr->msgid, lr->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	ft = (struct fec_tlv *)buf;

	if (len < sizeof(*ft) ||
	    (len - TLV_HDR_LEN) < ntohs(ft->length)) {
		session_shutdown(nbr, S_BAD_TLV_LEN, lr->msgid, lr->type);
		return (-1);
	}

	feclen = ntohs(ft->length);
	buf += sizeof(struct fec_tlv);
	len -= sizeof(struct fec_tlv);

	/* release may include optional label */
	if (len >= feclen) {
		struct label_tlv	*lt;

		lt = (struct label_tlv *)(buf + feclen);
		optlabel = tlv_decode_label(lt);
		if (optlabel == NO_LABEL) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lr->msgid,
			    lr->type);
			return (-1);
		}
	}

	bzero(&map, sizeof(map));
	map.messageid = lr->msgid;
	if (optlabel != NO_LABEL) {
		map.label = optlabel;
		map.flags = F_MAP_OPTLABEL;
	}
	do {
		if ((tlen = tlv_decode_fec_elm(buf, feclen, &addr_type,
		    &map.prefix, &map.prefixlen)) == -1) {
			session_shutdown(nbr, S_BAD_TLV_VAL, lr->msgid,
			    lr->type);
			return (-1);
		}

		if (addr_type == FEC_WILDCARD) {
			map.prefix = 0;
			map.prefixlen = 0;
			map.flags |= F_MAP_WILDCARD;

		} else
			map.flags &= ~F_MAP_WILDCARD;

		ldpe_imsg_compose_lde(IMSG_LABEL_RELEASE, nbr->peerid, 0, &map,
		    sizeof(map));

		buf += tlen;
		feclen -= tlen;
	} while (feclen > 0);

	nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(lr->length));
}

/* Label Abort Req Message */
void
send_labelabortreq(struct nbr *nbr)
{
	struct buf	*buf;
	u_int16_t	 size;

	if (nbr->iface->passive)
		return;

	log_debug("send_labelabortreq: neighbor ID %s", inet_ntoa(nbr->id));

	if ((buf = buf_open(LDP_MAX_LEN)) == NULL)
		fatal("send_labelabortreq");

	size = LDP_HDR_SIZE + sizeof(struct ldp_msg);

	gen_ldp_hdr(buf, nbr->iface, size);

	size -= LDP_HDR_SIZE;

	gen_msg_tlv(buf, MSG_TYPE_LABELABORTREQ, size);

	evbuf_enqueue(&nbr->wbuf, buf);
}

int
recv_labelabortreq(struct nbr *nbr, char *buf, u_int16_t len)
{
	struct ldp_msg	*la;

	log_debug("recv_labelabortreq: neighbor ID %s", inet_ntoa(nbr->id));

	if (nbr->state != NBR_STA_OPER)
		return (-1);

	la = (struct ldp_msg *)buf;

	if ((len - TLV_HDR_LEN) < ntohs(la->length)) {
		session_shutdown(nbr, S_BAD_MSG_LEN, la->msgid, la->type);
		return (-1);
	}

	buf += sizeof(struct ldp_msg);
	len -= sizeof(struct ldp_msg);

	/* XXX XXX XXX */

	nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

	return (ntohs(la->length));
}

/* Other TLV related functions */
void
gen_fec_tlv(struct buf *buf, u_int32_t prefix, u_int8_t prefixlen)
{
	struct fec_tlv	ft;
	u_int8_t	type;
	u_int16_t	family;
	u_int8_t	len;

	len = PREFIX_SIZE(prefixlen);
	ft.type = htons(TLV_TYPE_FEC);
	ft.length = htons(sizeof(type) + sizeof(family) + sizeof(prefixlen) +
	    len);

	buf_add(buf, &ft, sizeof(ft));

	type = FEC_PREFIX;
	family = htons(FEC_IPV4);

	buf_add(buf, &type, sizeof(type));
	buf_add(buf, &family, sizeof(family));
	buf_add(buf, &prefixlen, sizeof(prefixlen));
	if (len)
		buf_add(buf, &prefix, len);
}

void
gen_label_tlv(struct buf *buf, u_int32_t label)
{
	struct label_tlv	lt;

	lt.type = htons(TLV_TYPE_GENERICLABEL);
	lt.length = htons(sizeof(label));
	lt.label = htonl(label);

	buf_add(buf, &lt, sizeof(lt));
}

u_int32_t
tlv_decode_label(struct label_tlv *lt)
{
	if (lt->type != htons(TLV_TYPE_GENERICLABEL))
		return (NO_LABEL);

	if (ntohs(lt->length) != sizeof(lt->label))
		return (NO_LABEL);

	return (ntohl(lt->label));
}

int
tlv_decode_fec_elm(char *buf, u_int16_t len, u_int8_t *type, u_int32_t *prefix,
    u_int8_t *prefixlen)
{
	u_int16_t	family, off = 0;

	*type = *buf;
	off += sizeof(u_int8_t);

	if (*type == FEC_WILDCARD) {
		if (len == 0)
			return (off);
		else
			return (-1); /* XXX Malformed TLV Value */
	}

	if (*type != FEC_PREFIX)
		return (-1);	/* XXX "Unknown FEC" Notification */

	if (len < FEC_ELM_MIN_LEN)
		return (-1);	/* XXX Bad TLV Length */

	bcopy(buf + off, &family, sizeof(family));
	off += sizeof(family);

	if (family != htons(FEC_IPV4))
		return (-1);	/* XXX "Unsupported Address Family" */

	*prefixlen = buf[off];
	off += sizeof(u_int8_t);

	if (len < off + PREFIX_SIZE(*prefixlen))
		return (-1);	/* XXX Bad TLV Length */

	*prefix = 0;
	bcopy(buf + off, prefix, PREFIX_SIZE(*prefixlen));

	return (off + PREFIX_SIZE(*prefixlen));
}

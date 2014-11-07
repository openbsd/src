/*	$OpenBSD: ikev2_pld.c,v 1.46 2014/11/07 14:05:58 mikeb Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2014 Hans-Joerg Hoexer
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>

#include <openssl/sha.h>
#include <openssl/evp.h>

#include "iked.h"
#include "ikev2.h"
#include "eap.h"
#include "dh.h"

int	 ikev2_validate_pld(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *);
int	 ikev2_pld_payloads(struct iked *, struct iked_message *,
	    size_t, size_t, u_int);
int	 ikev2_validate_sa(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_sa_proposal *);
int	 ikev2_pld_sa(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_xform(struct iked_message *, size_t, size_t,
	    struct ikev2_transform *);
int	 ikev2_pld_xform(struct iked *, struct ikev2_sa_proposal *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_attr(struct iked_message *, size_t, size_t,
	    struct ikev2_attribute *);
int	 ikev2_pld_attr(struct iked *, struct ikev2_transform *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_ke(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_keyexchange *);
int	 ikev2_pld_ke(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_id(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_id *);
int	 ikev2_pld_id(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t, u_int);
int	 ikev2_validate_cert(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_cert *);
int	 ikev2_pld_cert(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_certreq(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_cert *);
int	 ikev2_pld_certreq(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_nonce(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *);
int	 ikev2_pld_nonce(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_notify(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_notify *);
int	 ikev2_pld_notify(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_delete(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_delete *);
int	 ikev2_pld_delete(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_ts(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_tsp *);
int	 ikev2_pld_ts(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t, u_int);
int	 ikev2_validate_auth(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_auth *);
int	 ikev2_pld_auth(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_pld_e(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t);
int	 ikev2_validate_cp(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct ikev2_cp *);
int	 ikev2_pld_cp(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_eap(struct iked_message *, size_t, size_t,
	    struct ikev2_payload *, struct eap_header *);
int	 ikev2_pld_eap(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);

int
ikev2_pld_parse(struct iked *env, struct ike_header *hdr,
    struct iked_message *msg, size_t offset)
{
	log_debug("%s: header ispi %s rspi %s"
	    " nextpayload %s version 0x%02x exchange %s flags 0x%02x"
	    " msgid %d length %d response %d", __func__,
	    print_spi(betoh64(hdr->ike_ispi), 8),
	    print_spi(betoh64(hdr->ike_rspi), 8),
	    print_map(hdr->ike_nextpayload, ikev2_payload_map),
	    hdr->ike_version,
	    print_map(hdr->ike_exchange, ikev2_exchange_map),
	    hdr->ike_flags,
	    betoh32(hdr->ike_msgid),
	    betoh32(hdr->ike_length),
	    msg->msg_response);

	if (ibuf_size(msg->msg_data) < betoh32(hdr->ike_length)) {
		log_debug("%s: short message", __func__);
		return (-1);
	}

	offset += sizeof(*hdr);

	return (ikev2_pld_payloads(env, msg, offset,
	    betoh32(hdr->ike_length), hdr->ike_nextpayload));
}

int
ikev2_validate_pld(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	/* We need at least the generic header. */
	if (left < sizeof(*pld)) {
		log_debug("%s: malformed payload: too short for generic "
		    "header (%zu < %zu)", __func__, left, sizeof(*pld));
		return (-1);
	}
	memcpy(pld, msgbuf + offset, sizeof(*pld));

	/*
	 * We need at least the specified number of bytes.
	 * pld_length is the full size of the payload including
	 * the generic payload header.
	 */
	pld_length = betoh16(pld->pld_length);
	if (left < pld_length) {
		log_debug("%s: malformed payload: shorter than specified "
		    "(%zu < %zu)", __func__, left, pld_length);
		return (-1);
	}
	/*
	 * Sanity check the specified payload size, it must
	 * be at last the size of the generic payload header.
	 */
	if (pld_length < sizeof(*pld)) {
		log_debug("%s: malformed payload: shorter than minimum "
		    "header size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld));
		return (-1);
	}

	return (0);
}

int
ikev2_pld_payloads(struct iked *env, struct iked_message *msg,
    size_t offset, size_t length, u_int payload)
{
	struct ikev2_payload	 pld;
	u_int			 e;
	int			 ret;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);
	size_t			 left;

	/* Check if message was decrypted in an E payload */
	e = msg->msg_e ? IKED_E : 0;

	while (payload != 0 && offset < length) {
		/* Bytes left in datagram. */
		left = length - offset;

		if (ikev2_validate_pld(msg, offset, left, &pld))
			return (-1);

		log_debug("%s: %spayload %s"
		    " nextpayload %s critical 0x%02x length %d",
		    __func__, e ? "decrypted " : "",
		    print_map(payload, ikev2_payload_map),
		    print_map(pld.pld_nextpayload, ikev2_payload_map),
		    pld.pld_reserved & IKEV2_CRITICAL_PAYLOAD,
		    betoh16(pld.pld_length));

		/* Skip over generic payload header. */
		offset += sizeof(pld);
		left -= sizeof(pld);
		ret = 0;

		switch (payload | e) {
		case IKEV2_PAYLOAD_SA:
		case IKEV2_PAYLOAD_SA | IKED_E:
			ret = ikev2_pld_sa(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_KE:
		case IKEV2_PAYLOAD_KE | IKED_E:
			ret = ikev2_pld_ke(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_IDi | IKED_E:
		case IKEV2_PAYLOAD_IDr | IKED_E:
			ret = ikev2_pld_id(env, &pld, msg, offset, left,
			    payload);
			break;
		case IKEV2_PAYLOAD_CERT | IKED_E:
			ret = ikev2_pld_cert(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_CERTREQ:
		case IKEV2_PAYLOAD_CERTREQ | IKED_E:
			ret = ikev2_pld_certreq(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_AUTH | IKED_E:
			ret = ikev2_pld_auth(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_NONCE:
		case IKEV2_PAYLOAD_NONCE | IKED_E:
			ret = ikev2_pld_nonce(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_NOTIFY:
		case IKEV2_PAYLOAD_NOTIFY | IKED_E:
			ret = ikev2_pld_notify(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_DELETE | IKED_E:
			ret = ikev2_pld_delete(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_TSi | IKED_E:
		case IKEV2_PAYLOAD_TSr | IKED_E:
			ret = ikev2_pld_ts(env, &pld, msg, offset, left,
			    payload);
			break;
		case IKEV2_PAYLOAD_SK:
			ret = ikev2_pld_e(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_CP | IKED_E:
			ret = ikev2_pld_cp(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_EAP | IKED_E:
			ret = ikev2_pld_eap(env, &pld, msg, offset, left);
			break;
		default:
			print_hex(msgbuf, offset,
			    betoh16(pld.pld_length) - sizeof(pld));
			break;
		}

		if (ret != 0 && ikev2_msg_frompeer(msg)) {
			(void)ikev2_send_informational(env, msg);
			return (-1);
		}

		/* Encrypted payload must appear last */
		if (payload == IKEV2_PAYLOAD_SK)
			return (0);

		payload = pld.pld_nextpayload;
		offset += betoh16(pld.pld_length) - sizeof(pld);
	}

	return (0);
}

int
ikev2_validate_sa(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_sa_proposal *sap)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length, sap_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*sap)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*sap));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*sap)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*sap));
		return (-1);
	}
	memcpy(sap, msgbuf + offset, sizeof(*sap));

	sap_length = betoh16(sap->sap_length);
	if (sap_length < sizeof(*sap)) {
		log_debug("%s: malformed payload: shorter than minimum header "
		    "size (%zu < %zu)", __func__, sap_length, sizeof(*sap));
		return (-1);
	}
	if (left < sap_length) {
		log_debug("%s: malformed payload: too long for actual payload "
		    "size (%zu < %zu)", __func__, left, sap_length);
		return (-1);
	}
	/*
	 * NB: There might be more proposals, we parse only the first one.
	 * This condition must never be true.
	 */
	if (pld_length - sizeof(*pld) < sap_length) {
		log_debug("%s: payload malformed: SA payload length mismatches "
		    "proposal substructure length (%lu < %zu)", __func__,
		    pld_length - sizeof(*pld), sap_length);
		return (-1);
	}
	/*
	 * If there is only one proposal, sap_length must be the
	 * total payload size.
	 */
	if (!sap->sap_more && ((pld_length - sizeof(*pld)) != sap_length)) {
		log_debug("%s: payload malformed: SA payload length mismatches "
		    "single proposal substructure length (%lu != %zu)",
		    __func__, pld_length - sizeof(*pld), sap_length);
		return (-1);
	}
	/*
	 * If there are more than one proposal, there must be bytes
	 * left in the payload.
	 */
	if (sap->sap_more && ((pld_length - sizeof(*pld)) <= sap_length)) {
		log_debug("%s: payload malformed: SA payload too small for "
		    "further proposals (%zu <= %zu)", __func__,
		    pld_length - sizeof(*pld), sap_length);
		return (-1);
	}
	return (0);
}

/*
 * NB: This function parses both the SA header and the first proposal.
 * Additional proposals are ignored.
 */
int
ikev2_pld_sa(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_sa_proposal	 sap;
	struct iked_proposal		*prop = NULL;
	u_int32_t			 spi32;
	u_int64_t			 spi = 0, spi64;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);
	struct iked_proposals		*props;
	size_t				 total;

	if (ikev2_validate_sa(msg, offset, left, pld, &sap))
		return (-1);

	if (sap.sap_more)
		log_debug("%s: more than one proposal specified", __func__);

	/* Assumed size of the first proposals, including SPI if present. */
	total = (betoh16(sap.sap_length) - sizeof(sap));

	props = &msg->msg_parent->msg_proposals;

	offset += sizeof(sap);
	left -= sizeof(sap);

	if (sap.sap_spisize) {
		if (left < sap.sap_spisize) {
			log_debug("%s: malformed payload: SPI larger than "
			    "actual payload (%zu < %d)", __func__, left,
			    sap.sap_spisize);
			return (-1);
		}
		if (total < sap.sap_spisize) {
			log_debug("%s: malformed payload: SPI larger than "
			    "proposal (%zu < %d)", __func__, total,
			    sap.sap_spisize);
			return (-1);
		}
		if (total < sap.sap_spisize) {
			log_debug("%s: malformed payload: SPI too large "
			    "(%zu < %d)", __func__, total, sap.sap_spisize);
			return (-1);
		}
		switch (sap.sap_spisize) {
		case 4:
			memcpy(&spi32, msgbuf + offset, 4);
			spi = betoh32(spi32);
			break;
		case 8:
			memcpy(&spi64, msgbuf + offset, 8);
			spi = betoh64(spi64);
			break;
		default:
			log_debug("%s: unsupported SPI size %d",
			    __func__, sap.sap_spisize);
			return (-1);
		}

		offset += sap.sap_spisize;
		left -= sap.sap_spisize;

		/* Assumed size of the proposal, now without SPI. */
		total -= sap.sap_spisize;
	}

	/*
	 * As we verified sanity of packet headers, this check will
	 * be always false, but just to be sure we keep it.
	 */
	if (left < total) {
		log_debug("%s: payload malformed: too long for payload "
		    "(%zu < %zu)", __func__, left, total);
		return (-1);
	}

	log_debug("%s: more %d reserved %d length %d"
	    " proposal #%d protoid %s spisize %d xforms %d spi %s",
	    __func__, sap.sap_more, sap.sap_reserved,
	    betoh16(sap.sap_length), sap.sap_proposalnr,
	    print_map(sap.sap_protoid, ikev2_saproto_map), sap.sap_spisize,
	    sap.sap_transforms, print_spi(spi, sap.sap_spisize));

	if (ikev2_msg_frompeer(msg)) {
		if ((msg->msg_parent->msg_prop = config_add_proposal(props,
		    sap.sap_proposalnr, sap.sap_protoid)) == NULL) {
			log_debug("%s: invalid proposal", __func__);
			return (-1);
		}
		prop = msg->msg_parent->msg_prop;
		prop->prop_peerspi.spi = spi;
		prop->prop_peerspi.spi_protoid = sap.sap_protoid;
		prop->prop_peerspi.spi_size = sap.sap_spisize;

		prop->prop_localspi.spi_protoid = sap.sap_protoid;
		prop->prop_localspi.spi_size = sap.sap_spisize;
	}

	/*
	 * Parse the attached transforms
	 */
	if (sap.sap_transforms &&
	    ikev2_pld_xform(env, &sap, msg, offset, total) != 0) {
		log_debug("%s: invalid proposal transforms", __func__);
		return (-1);
	}

	return (0);
}

int
ikev2_validate_xform(struct iked_message *msg, size_t offset, size_t total,
    struct ikev2_transform *xfrm)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 xfrm_length;

	if (total < sizeof(*xfrm)) {
		log_debug("%s: payload malformed: too short for header "
		    "(%zu < %zu)", __func__, total, sizeof(*xfrm));
		return (-1);
	}
	memcpy(xfrm, msgbuf + offset, sizeof(*xfrm));

	xfrm_length = betoh16(xfrm->xfrm_length);
	if (xfrm_length < sizeof(*xfrm)) {
		log_debug("%s: payload malformed: shorter than minimal header "
		    "(%zu < %zu)", __func__, xfrm_length, sizeof(*xfrm));
		return (-1);
	}
	if (total < xfrm_length) {
		log_debug("%s: malformed payload: too long for payload size "
		    "(%zu < %zu)", __func__, total, xfrm_length);
		return (-1);
	}

	return (0);
}

int
ikev2_pld_xform(struct iked *env, struct ikev2_sa_proposal *sap,
    struct iked_message *msg, size_t offset, size_t total)
{
	struct ikev2_transform		 xfrm;
	char				 id[BUFSIZ];
	int				 ret = 0;
	size_t				 xfrm_length;

	if (ikev2_validate_xform(msg, offset, total, &xfrm))
		return (-1);

	xfrm_length = betoh16(xfrm.xfrm_length);

	switch (xfrm.xfrm_type) {
	case IKEV2_XFORMTYPE_ENCR:
		strlcpy(id, print_map(betoh16(xfrm.xfrm_id),
		    ikev2_xformencr_map), sizeof(id));
		break;
	case IKEV2_XFORMTYPE_PRF:
		strlcpy(id, print_map(betoh16(xfrm.xfrm_id),
		    ikev2_xformprf_map), sizeof(id));
		break;
	case IKEV2_XFORMTYPE_INTEGR:
		strlcpy(id, print_map(betoh16(xfrm.xfrm_id),
		    ikev2_xformauth_map), sizeof(id));
		break;
	case IKEV2_XFORMTYPE_DH:
		strlcpy(id, print_map(betoh16(xfrm.xfrm_id),
		    ikev2_xformdh_map), sizeof(id));
		break;
	case IKEV2_XFORMTYPE_ESN:
		strlcpy(id, print_map(betoh16(xfrm.xfrm_id),
		    ikev2_xformesn_map), sizeof(id));
		break;
	default:
		snprintf(id, sizeof(id), "<%d>", betoh16(xfrm.xfrm_id));
		break;
	}

	log_debug("%s: more %d reserved %d length %zu"
	    " type %s id %s",
	    __func__, xfrm.xfrm_more, xfrm.xfrm_reserved, xfrm_length,
	    print_map(xfrm.xfrm_type, ikev2_xformtype_map), id);

	/*
	 * Parse transform attributes, if available
	 */
	msg->msg_attrlength = 0;
	if (xfrm_length > sizeof(xfrm)) {
		if (ikev2_pld_attr(env, &xfrm, msg, offset + sizeof(xfrm),
		    xfrm_length - sizeof(xfrm)) != 0) {
			return (-1);
		}
	}

	if (ikev2_msg_frompeer(msg)) {
		if (config_add_transform(msg->msg_parent->msg_prop,
		    xfrm.xfrm_type, betoh16(xfrm.xfrm_id),
		    msg->msg_attrlength, msg->msg_attrlength) == NULL) {
			log_debug("%s: failed to add transform", __func__);
			return (-1);
		}
	}

	/* Next transform */
	offset += xfrm_length;
	total -= xfrm_length;
	if (xfrm.xfrm_more == IKEV2_XFORM_MORE)
		ret = ikev2_pld_xform(env, sap, msg, offset, total);
	else if (total != 0) {
		/* No more transforms but still some data left. */
		log_debug("%s: less data than specified, %zu bytes left",
		    __func__, total);
		ret = -1;
	}

	return (ret);
}

int
ikev2_validate_attr(struct iked_message *msg, size_t offset, size_t total,
    struct ikev2_attribute *attr)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);

	if (total < sizeof(*attr)) {
		log_debug("%s: payload malformed: too short for header "
		    "(%zu < %zu)", __func__, total, sizeof(*attr));
		return (-1);
	}
	memcpy(attr, msgbuf + offset, sizeof(*attr));

	return (0);
}

int
ikev2_pld_attr(struct iked *env, struct ikev2_transform *xfrm,
    struct iked_message *msg, size_t offset, size_t total)
{
	struct ikev2_attribute		 attr;
	u_int				 type;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);
	int				 ret = 0;
	size_t				 attr_length;

	if (ikev2_validate_attr(msg, offset, total, &attr))
		return (-1);

	type = betoh16(attr.attr_type) & ~IKEV2_ATTRAF_TV;

	log_debug("%s: attribute type %s length %d total %zu",
	    __func__, print_map(type, ikev2_attrtype_map),
	    betoh16(attr.attr_length), total);

	if (betoh16(attr.attr_type) & IKEV2_ATTRAF_TV) {
		/* Type-Value attribute */
		offset += sizeof(attr);
		total -= sizeof(attr);

		if (type == IKEV2_ATTRTYPE_KEY_LENGTH)
			msg->msg_attrlength = betoh16(attr.attr_length);
	} else {
		/* Type-Length-Value attribute */
		attr_length = betoh16(attr.attr_length);
		if (attr_length < sizeof(attr)) {
			log_debug("%s: payload malformed: shorter than "
			    "minimal header (%zu < %zu)", __func__,
			    attr_length, sizeof(attr));
			return (-1);
		}
		if (total < attr_length) {
			log_debug("%s: payload malformed: attribute larger "
			    "than actual payload (%zu < %zu)", __func__,
			    total, attr_length);
			return (-1);
		}
		print_hex(msgbuf, offset + sizeof(attr),
		    attr_length - sizeof(attr));
		offset += attr_length;
		total -= attr_length;
	}

	if (total > 0) {
		/* Next attribute */
		ret = ikev2_pld_attr(env, xfrm, msg, offset, total);
	}

	return (ret);
}

int
ikev2_validate_ke(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_keyexchange *kex)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*kex)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*kex));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*kex)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*kex));
		return (-1);
	}
	memcpy(kex, msgbuf + offset, sizeof(*kex));

	return (0);
}

int
ikev2_pld_ke(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_keyexchange	 kex;
	u_int8_t			*buf;
	size_t				 len;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_ke(msg, offset, left, pld, &kex))
		return (-1);

	log_debug("%s: dh group %s reserved %d", __func__,
	    print_map(betoh16(kex.kex_dhgroup), ikev2_xformdh_map),
	    betoh16(kex.kex_reserved));

	buf = msgbuf + offset + sizeof(kex);
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(kex);

	if (len == 0) {
		log_debug("%s: malformed payload: no KE data given", __func__);
		return (-1);
	}
	/* This will actually be caught by earlier checks. */
	if (left < len) {
		log_debug("%s: malformed payload: smaller than specified "
		     "(%zu < %zu)", __func__, left, len);
		return (-1);
	}

	print_hex(buf, 0, len);

	if (ikev2_msg_frompeer(msg)) {
		ibuf_release(msg->msg_parent->msg_ke);
		if ((msg->msg_parent->msg_ke = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get exchange", __func__);
			return (-1);
		}
	}

	return (0);
}

int
ikev2_validate_id(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_id *id)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*id)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*id));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*id)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*id));
		return (-1);
	}
	memcpy(id, msgbuf + offset, sizeof(*id));

	return (0);
}

int
ikev2_pld_id(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left, u_int payload)
{
	u_int8_t			*ptr;
	struct ikev2_id			 id;
	size_t				 len;
	struct iked_id			*idp, idb;
	struct iked_sa			*sa = msg->msg_sa;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);
	char				 idstr[IKED_ID_SIZE];

	if (ikev2_validate_id(msg, offset, left, pld, &id))
		return (-1);

	bzero(&idb, sizeof(idb));

	/* Don't strip the Id payload header */
	ptr = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);

	idb.id_type = id.id_type;
	idb.id_offset = sizeof(id);
	if ((idb.id_buf = ibuf_new(ptr, len)) == NULL)
		return (-1);

	if (ikev2_print_id(&idb, idstr, sizeof(idstr)) == -1) {
		log_debug("%s: malformed id", __func__);
		return (-1);
	}

	log_debug("%s: id %s length %zu", __func__, idstr, len);

	if (!ikev2_msg_frompeer(msg)) {
		ibuf_release(idb.id_buf);
		return (0);
	}

	if (!((sa->sa_hdr.sh_initiator && payload == IKEV2_PAYLOAD_IDr) ||
	    (!sa->sa_hdr.sh_initiator && payload == IKEV2_PAYLOAD_IDi))) {
		log_debug("%s: unexpected id payload", __func__);
		return (0);
	}

	idp = &msg->msg_parent->msg_id;
	if (idp->id_type) {
		log_debug("%s: duplicate id payload", __func__);
		return (-1);
	}

	idp->id_buf = idb.id_buf;
	idp->id_offset = idb.id_offset;
	idp->id_type = idb.id_type;

	return (0);
}

int
ikev2_validate_cert(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_cert *cert)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*cert)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*cert));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*cert)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*cert));
		return (-1);
	}
	memcpy(cert, msgbuf + offset, sizeof(*cert));

	return (0);
}

int
ikev2_pld_cert(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_cert		 cert;
	u_int8_t			*buf;
	size_t				 len;
	struct iked_id			*certid;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_cert(msg, offset, left, pld, &cert))
		return (-1);
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cert);

	log_debug("%s: type %s length %zu",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	certid = &msg->msg_parent->msg_cert;
	if (certid->id_type) {
		log_debug("%s: duplicate cert payload", __func__);
		return (-1);
	}

	if ((certid->id_buf = ibuf_new(buf, len)) == NULL) {
		log_debug("%s: failed to save cert", __func__);
		return (-1);
	}
	certid->id_type = cert.cert_type;
	certid->id_offset = 0;

	return (0);
}

int
ikev2_validate_certreq(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_cert *cert)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*cert)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*cert));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*cert)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*cert));
		return (-1);
	}
	memcpy(cert, msgbuf + offset, sizeof(*cert));

	return (0);
}

int
ikev2_pld_certreq(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct iked_sa			*sa = msg->msg_sa;
	struct ikev2_cert		 cert;
	u_int8_t			*buf;
	ssize_t				 len;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_certreq(msg, offset, left, pld, &cert))
		return (-1);
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cert);

	log_debug("%s: type %s length %zd",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);

	/* This will actually be caught by earlier checks. */
	if (len < 0) {
		log_debug("%s: invalid certificate request length", __func__);
		return (-1);
	}

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (cert.cert_type == IKEV2_CERT_X509_CERT) {
		if (!len || (len % SHA_DIGEST_LENGTH) != 0) {
			log_debug("%s: invalid certificate request", __func__);
			return (-1);
		}
	}

	if (msg->msg_sa == NULL)
		return (-1);

	/* Optional certreq for PSK */
	if (sa->sa_hdr.sh_initiator)
		sa->sa_stateinit |= IKED_REQ_CERT;
	else
		sa->sa_statevalid |= IKED_REQ_CERT;

	ca_setreq(env, &sa->sa_hdr, &sa->sa_policy->pol_localid,
	    cert.cert_type, buf, len, PROC_CERT);

	return (0);
}

int
ikev2_validate_auth(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_auth *auth)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*auth)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*auth));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*auth)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*auth));
		return (-1);
	}
	memcpy(auth, msgbuf + offset, sizeof(*auth));

	return (0);
}

int
ikev2_pld_auth(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_auth		 auth;
	struct iked_id			*idp;
	u_int8_t			*buf;
	size_t				 len;
	struct iked_sa			*sa = msg->msg_sa;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_auth(msg, offset, left, pld, &auth))
		return (-1);
	offset += sizeof(auth);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(auth);

	log_debug("%s: method %s length %zu",
	    __func__, print_map(auth.auth_method, ikev2_auth_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	/* The AUTH payload indicates if the responder wants EAP or not */
	if (!sa_stateok(sa, IKEV2_STATE_EAP))
		sa_state(env, sa, IKEV2_STATE_AUTH_REQUEST);

	idp = &msg->msg_parent->msg_auth;
	if (idp->id_type) {
		log_debug("%s: duplicate auth payload", __func__);
		return (-1);
	}

	ibuf_release(idp->id_buf);
	idp->id_type = auth.auth_method;
	idp->id_offset = 0;
	if ((idp->id_buf = ibuf_new(buf, len)) == NULL)
		return (-1);

	return (0);
}

int
ikev2_validate_nonce(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld)
{
	size_t		 pld_length;

	/* This will actually be caught by earlier checks. */
	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld));
		return (-1);
	}

	return (0);
}

int
ikev2_pld_nonce(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	size_t		 len;
	u_int8_t	*buf;
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_nonce(msg, offset, left, pld))
		return (-1);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);

	if (len == 0) {
		log_debug("%s: malformed payload: no NONCE given", __func__);
		return (-1);
	}
	/* This will actually be caught by earlier checks. */
	if (left < len) {
		log_debug("%s: malformed payload: smaller than specified "
		    "(%zu < %zu)", __func__, left, len);
		return (-1);
	}

	print_hex(buf, 0, len);

	if (ikev2_msg_frompeer(msg)) {
		ibuf_release(msg->msg_nonce);
		if ((msg->msg_nonce = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get peer nonce", __func__);
			return (-1);
		}
		msg->msg_parent->msg_nonce = msg->msg_nonce;
	}

	return (0);
}

int
ikev2_validate_notify(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_notify *n)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*n)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*n));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*n)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*n));
		return (-1);
	}
	memcpy(n, msgbuf + offset, sizeof(*n));

	return (0);
}

int
ikev2_pld_notify(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_notify	 n;
	u_int8_t		*buf, md[SHA_DIGEST_LENGTH];
	size_t			 len;
	u_int32_t		 spi32;
	u_int64_t		 spi64;
	struct iked_spi		*rekey;
	u_int16_t		 type;
	u_int16_t		 group;
	u_int16_t		 cpi;
	u_int8_t		 transform;

	if (ikev2_validate_notify(msg, offset, left, pld, &n))
		return (-1);
	type = betoh16(n.n_type);

	log_debug("%s: protoid %s spisize %d type %s",
	    __func__,
	    print_map(n.n_protoid, ikev2_saproto_map), n.n_spisize,
	    print_map(type, ikev2_n_map));

	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(n);
	if ((buf = ibuf_seek(msg->msg_data, offset + sizeof(n), len)) == NULL)
		return (-1);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	switch (type) {
	case IKEV2_N_NAT_DETECTION_SOURCE_IP:
	case IKEV2_N_NAT_DETECTION_DESTINATION_IP:
		if (len != sizeof(md)) {
			log_debug("%s: malformed payload: hash size mismatch"
			    " (%zu != %zu)", __func__, len, sizeof(md));
			return (-1);
		}
		if (ikev2_nat_detection(env, msg, md, sizeof(md), type) == -1)
			return (-1);
		if (memcmp(buf, md, len) != 0) {
			log_debug("%s: %s detected NAT, enabling "
			    "UDP encapsulation", __func__,
			    print_map(type, ikev2_n_map));

			/*
			 * Enable UDP encapsulation of ESP packages if
			 * the check detected NAT.
			 */
			if (msg->msg_sa != NULL)
				msg->msg_sa->sa_udpencap = 1;
		}
		print_hex(md, 0, sizeof(md));
		break;
	case IKEV2_N_INVALID_KE_PAYLOAD:
		if (sa_stateok(msg->msg_sa, IKEV2_STATE_VALID) &&
		    !msg->msg_e) {
			log_debug("%s: INVALID_KE_PAYLOAD not encrypted",
			    __func__);
			return (-1);
		}
		if (len != sizeof(group)) {
			log_debug("%s: malformed payload: group size mismatch"
			    " (%zu != %zu)", __func__, len, sizeof(group));
			return (-1);
		}
		/* XXX chould also happen for PFS */
		if (!msg->msg_sa->sa_hdr.sh_initiator) {
			log_debug("%s: not an initiator", __func__);
			sa_state(env, msg->msg_sa, IKEV2_STATE_CLOSED);
			msg->msg_sa = NULL;
			return (-1);
		}
		memcpy(&group, buf, len);
		group = betoh16(group);
		if ((msg->msg_policy->pol_peerdh = group_get(group))
		    == NULL) {
			log_debug("%s: unable to select DH group %d", __func__,
			    group);
			return (-1);
		}
		log_debug("%s: responder selected DH group %d", __func__,
		    group);
		sa_state(env, msg->msg_sa, IKEV2_STATE_CLOSED);
		msg->msg_sa = NULL;
		/* XXX chould also happen for PFS so we have to check state XXX*/
		timer_set(env, &env->sc_inittmr, ikev2_init_ike_sa, NULL);
		timer_add(env, &env->sc_inittmr, IKED_INITIATOR_INITIAL);
		break;
	case IKEV2_N_NO_ADDITIONAL_SAS:
		if (!msg->msg_e) {
			log_debug("%s: NO_ADDITIONAL_SAS not encrypted",
			    __func__);
			return (-1);
		}
		/* This makes sense for Child SAs only atm */
		if (msg->msg_sa->sa_stateflags & IKED_REQ_CHILDSA) {
			ikev2_disable_rekeying(env, msg->msg_sa);
			msg->msg_sa->sa_stateflags &= ~IKED_REQ_CHILDSA;
		}
		break;
	case IKEV2_N_REKEY_SA:
		if (!msg->msg_e) {
			log_debug("%s: N_REKEY_SA not encrypted", __func__);
			return (-1);
		}
		if (len != n.n_spisize) {
			log_debug("%s: malformed notification", __func__);
			return (-1);
		}
		rekey = &msg->msg_parent->msg_rekey;
		if (rekey->spi != 0) {
			log_debug("%s: rekeying of multiple SAs not supported",
			    __func__);
			return (-1);
		}
		switch (n.n_spisize) {
		case 4:
			memcpy(&spi32, buf, len);
			rekey->spi = betoh32(spi32);
			break;
		case 8:
			memcpy(&spi64, buf, len);
			rekey->spi = betoh64(spi64);
			break;
		default:
			log_debug("%s: invalid spi size %d", __func__,
			    n.n_spisize);
			return (-1);
		}
		rekey->spi_size = n.n_spisize;
		rekey->spi_protoid = n.n_protoid;

		log_debug("%s: rekey %s spi %s", __func__,
		    print_map(n.n_protoid, ikev2_saproto_map),
		    print_spi(rekey->spi, n.n_spisize));
		break;
	case IKEV2_N_IPCOMP_SUPPORTED:
		if (!msg->msg_e) {
			log_debug("%s: N_IPCOMP_SUPPORTED not encrypted",
			    __func__);
			return (-1);
		}
		if (len < sizeof(cpi) + sizeof(transform)) {
			log_debug("%s: ignoring malformed ipcomp notification",
			    __func__);
			return (0);
		}
		memcpy(&cpi, buf, sizeof(cpi));
		memcpy(&transform, buf + sizeof(cpi), sizeof(transform));
		log_debug("%s: cpi 0x%x, transform %s, len %zu", __func__,
		    betoh16(cpi), print_map(transform, ikev2_ipcomp_map), len);
		/* we only support deflate */
		if ((msg->msg_policy->pol_flags & IKED_POLICY_IPCOMP) &&
		    (transform == IKEV2_IPCOMP_DEFLATE)) {
			msg->msg_sa->sa_ipcomp = transform;
			msg->msg_sa->sa_cpi_out = betoh16(cpi);
		}
		break;
	}

	return (0);
}

int
ikev2_validate_delete(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_delete *del)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*del)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*del));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*del)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*del));
		return (-1);
	}
	memcpy(del, msgbuf + offset, sizeof(*del));

	return (0);
}

int
ikev2_pld_delete(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct iked_childsa	**peersas = NULL;
	struct iked_sa		*sa = msg->msg_sa;
	struct ikev2_delete	 del, *localdel;
	struct ibuf		*resp = NULL;
	u_int64_t		*localspi = NULL;
	u_int64_t		 spi64, spi = 0;
	u_int32_t		 spi32;
	u_int8_t		*buf, *msgbuf = ibuf_data(msg->msg_data);
	size_t			 found = 0, failed = 0;
	int			 cnt, i, len, sz, ret = -1;

	/* Skip if it's a response, then we don't have to deal with it */
	if (ikev2_msg_frompeer(msg) &&
	    msg->msg_parent->msg_response)
		return (0);

	if (ikev2_validate_delete(msg, offset, left, pld, &del))
		return (-1);
	cnt = betoh16(del.del_nspi);
	sz = del.del_spisize;

	log_debug("%s: proto %s spisize %d nspi %d",
	    __func__, print_map(del.del_protoid, ikev2_saproto_map),
	    sz, cnt);

	buf = msgbuf + offset + sizeof(del);
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(del);

	print_hex(buf, 0, len);

	switch (sz) {
	case 4:
	case 8:
		break;
	default:
		if (del.del_protoid != IKEV2_SAPROTO_IKE) {
			log_debug("%s: invalid SPI size", __func__);
			return (-1);
		}
		if (ikev2_msg_frompeer(msg)) {
			/* Send an empty informational response */
			if ((resp = ibuf_static()) == NULL)
				goto done;
			ret = ikev2_send_ike_e(env, sa, resp,
			    IKEV2_PAYLOAD_NONE,
			    IKEV2_EXCHANGE_INFORMATIONAL, 1);
			msg->msg_parent->msg_responded = 1;
			ibuf_release(resp);
			sa_state(env, sa, IKEV2_STATE_CLOSED);
		} else {
			/*
			 * We're sending a delete message. Upper layer
			 * must deal with deletion of the IKE SA.
			 */
			ret = 0;
		}
		return (ret);
	}

	if ((len / sz) != cnt) {
		log_debug("%s: invalid payload length %d/%d != %d",
		    __func__, len, sz, cnt);
		return (-1);
	}

	if (ikev2_msg_frompeer(msg) &&
	    ((peersas = calloc(cnt, sizeof(struct iked_childsa *))) == NULL ||
	     (localspi = calloc(cnt, sizeof(u_int64_t))) == NULL)) {
		log_warn("%s", __func__);
		goto done;
	}

	for (i = 0; i < cnt; i++) {
		switch (sz) {
		case 4:
			memcpy(&spi32, buf + (i * sz), sizeof(spi32));
			spi = betoh32(spi32);
			break;
		case 8:
			memcpy(&spi64, buf + (i * sz), sizeof(spi64));
			spi = betoh64(spi64);
			break;
		}

		log_debug("%s: spi %s", __func__, print_spi(spi, sz));

		if (peersas == NULL || sa == NULL)
			continue;

		if ((peersas[i] = childsa_lookup(sa, spi,
		    del.del_protoid)) == NULL) {
			log_warnx("%s: CHILD SA doesn't exist for spi %s",
			    __func__, print_spi(spi, del.del_spisize));
			continue;
		}

		if (ikev2_childsa_delete(env, sa, del.del_protoid, spi,
		    &localspi[i], 0) == -1)
			failed++;
		else
			found++;

		/*
		 * Flows are left in the require mode so that it would be
		 * possible to quickly negotiate a new Child SA
		 */
	}

	/* Parsed outgoing message? */
	if (!ikev2_msg_frompeer(msg))
		goto done;

	if (msg->msg_parent->msg_response) {
		ret = 0;
		goto done;
	}

	/* Response to the INFORMATIONAL with Delete payload */

	if ((resp = ibuf_static()) == NULL)
		goto done;

	if (found) {
		if ((localdel = ibuf_advance(resp, sizeof(*localdel))) == NULL)
			goto done;

		localdel->del_protoid = del.del_protoid;
		localdel->del_spisize = del.del_spisize;
		localdel->del_nspi = htobe16(found);

		for (i = 0; i < cnt; i++) {
			switch (sz) {
			case 4:
				spi32 = htobe32(localspi[i]);
				if (ibuf_add(resp, &spi32, sizeof(spi32)) != 0)
					goto done;
				break;
			case 8:
				spi64 = htobe64(localspi[i]);
				if (ibuf_add(resp, &spi64, sizeof(spi64)) != 0)
					goto done;
				break;
			}
		}

		log_warnx("%s: deleted %zu spis", __func__, found);
	}

	if (found) {
		ret = ikev2_send_ike_e(env, sa, resp, IKEV2_PAYLOAD_DELETE,
		    IKEV2_EXCHANGE_INFORMATIONAL, 1);
		msg->msg_parent->msg_responded = 1;
	} else {
		/* XXX should we send an INVALID_SPI notification? */
		ret = 0;
	}

 done:
	if (localspi)
		free(localspi);
	if (peersas)
		free(peersas);
	ibuf_release(resp);
	return (ret);
}

int
ikev2_validate_ts(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_tsp *tsp)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*tsp)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*tsp));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*tsp)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*tsp));
		return (-1);
	}
	memcpy(tsp, msgbuf + offset, sizeof(*tsp));

	return (0);
}

int
ikev2_pld_ts(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left, u_int payload)
{
	struct ikev2_tsp		 tsp;
	struct ikev2_ts			 ts;
	size_t				 len, i;
	struct sockaddr_in		 s4;
	struct sockaddr_in6		 s6;
	u_int8_t			 buf[2][128];
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_ts(msg, offset, left, pld, &tsp))
		return (-1);
	offset += sizeof(tsp);

	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(tsp);

	log_debug("%s: count %d length %zu", __func__,
	    tsp.tsp_count, len);

	for (i = 0; i < tsp.tsp_count; i++) {
		memcpy(&ts, msgbuf + offset, sizeof(ts));

		log_debug("%s: type %s protoid %u length %d "
		    "startport %u endport %u", __func__,
		    print_map(ts.ts_type, ikev2_ts_map),
		    ts.ts_protoid, betoh16(ts.ts_length),
		    betoh16(ts.ts_startport),
		    betoh16(ts.ts_endport));

		switch (ts.ts_type) {
		case IKEV2_TS_IPV4_ADDR_RANGE:
			bzero(&s4, sizeof(s4));
			s4.sin_family = AF_INET;
			s4.sin_len = sizeof(s4);
			memcpy(&s4.sin_addr.s_addr,
			    msgbuf + offset + sizeof(ts), 4);
			print_host((struct sockaddr *)&s4,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s4.sin_addr.s_addr,
			    msgbuf + offset + sizeof(ts) + 4, 4);
			print_host((struct sockaddr *)&s4,
			    (char *)buf[1], sizeof(buf[1]));
			log_debug("%s: start %s end %s", __func__,
			    buf[0], buf[1]);
			break;
		case IKEV2_TS_IPV6_ADDR_RANGE:
			bzero(&s6, sizeof(s6));
			s6.sin6_family = AF_INET6;
			s6.sin6_len = sizeof(s6);
			memcpy(&s6.sin6_addr,
			    msgbuf + offset + sizeof(ts), 16);
			print_host((struct sockaddr *)&s6,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s6.sin6_addr,
			    msgbuf + offset + sizeof(ts) + 16, 16);
			print_host((struct sockaddr *)&s6,
			    (char *)buf[1], sizeof(buf[1]));
			log_debug("%s: start %s end %s", __func__,
			    buf[0], buf[1]);
			break;
		default:
			break;
		}

		offset += betoh16(ts.ts_length);
	}

	return (0);
}

int
ikev2_pld_e(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset)
{
	struct iked_sa		*sa = msg->msg_sa;
	struct ibuf		*e = NULL;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);
	struct iked_message	 emsg;
	u_int8_t		*buf;
	size_t			 len;
	int			 ret = -1;

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);

	if ((e = ibuf_new(buf, len)) == NULL)
		goto done;

	if (ikev2_msg_frompeer(msg)) {
		e = ikev2_msg_decrypt(env, msg->msg_sa, msg->msg_data, e);
	} else {
		sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;
		e = ikev2_msg_decrypt(env, msg->msg_sa, msg->msg_data, e);
		sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;
	}

	if (e == NULL)
		goto done;

	/*
	 * Parse decrypted payload
	 */
	bzero(&emsg, sizeof(emsg));
	memcpy(&emsg, msg, sizeof(*msg));
	emsg.msg_data = e;
	emsg.msg_e = 1;
	emsg.msg_parent = msg;
	TAILQ_INIT(&emsg.msg_proposals);

	ret = ikev2_pld_payloads(env, &emsg, 0, ibuf_size(e),
	    pld->pld_nextpayload);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_validate_cp(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct ikev2_cp *cp)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*cp)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*cp));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*cp)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*cp));
		return (-1);
	}
	memcpy(cp, msgbuf + offset, sizeof(*cp));

	return (0);
}

int
ikev2_pld_cp(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_cp		 cp;
	struct ikev2_cfg	*cfg;
	u_int8_t		*buf;
	size_t			 len, i;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa		*sa = msg->msg_sa;

	if (ikev2_validate_cp(msg, offset, left, pld, &cp))
		return (-1);
	offset += sizeof(cp);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cp);

	log_debug("%s: type %s length %zu",
	    __func__, print_map(cp.cp_type, ikev2_cp_map), len);
	print_hex(buf, 0, len);

	for (i = 0; i < len;) {
		cfg = (struct ikev2_cfg *)(buf + i);

		log_debug("%s: %s 0x%04x length %d", __func__,
		    print_map(betoh16(cfg->cfg_type), ikev2_cfg_map),
		    betoh16(cfg->cfg_type),
		    betoh16(cfg->cfg_length));

		i += betoh16(cfg->cfg_length) + sizeof(*cfg);
	}

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (sa)
		sa->sa_cp = cp.cp_type;

	return (0);
}

int
ikev2_validate_eap(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_payload *pld, struct eap_header *hdr)
{
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	size_t		 pld_length;

	pld_length = betoh16(pld->pld_length);
	if (pld_length < sizeof(*pld) + sizeof(*hdr)) {
		log_debug("%s: malformed payload: specified length smaller "
		    "than minimum size (%zu < %zu)", __func__, pld_length,
		    sizeof(*pld) + sizeof(*hdr));
		return (-1);
	}

	/* This will actually be caught by earlier checks. */
	if (left < sizeof(*hdr)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, left, sizeof(*hdr));
		return (-1);
	}
	memcpy(hdr, msgbuf + offset, sizeof(*hdr));

	return (0);
}

int
ikev2_pld_eap(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct eap_header		 hdr;
	struct eap_message		*eap = NULL;
	struct iked_sa			*sa = msg->msg_sa;
	size_t				 len;

	if (ikev2_validate_eap(msg, offset, left, pld, &hdr))
		return (-1);
	len = betoh16(hdr.eap_length);

	if (len < sizeof(*eap)) {
		log_info("%s: %s id %d length %d", __func__,
		    print_map(hdr.eap_code, eap_code_map),
		    hdr.eap_id, betoh16(hdr.eap_length));
	} else {
		/* Now try to get the indicated length */
		if ((eap = ibuf_seek(msg->msg_data, offset, len)) == NULL) {
			log_debug("%s: invalid EAP length", __func__);
			return (-1);
		}

		log_info("%s: %s id %d length %d EAP-%s", __func__,
		    print_map(eap->eap_code, eap_code_map),
		    eap->eap_id, betoh16(eap->eap_length),
		    print_map(eap->eap_type, eap_type_map));

		if (eap_parse(env, sa, eap, msg->msg_response) == -1)
			return (-1);
	}

	return (0);
}

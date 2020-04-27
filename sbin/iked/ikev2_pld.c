/*	$OpenBSD: ikev2_pld.c,v 1.85 2020/04/27 19:28:13 tobhe Exp $	*/

/*
 * Copyright (c) 2019 Tobias Heider <tobias.heider@stusta.de>
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

#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
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
	    size_t, size_t, unsigned int);
int	 ikev2_validate_sa(struct iked_message *, size_t, size_t,
	    struct ikev2_sa_proposal *);
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
	    struct ikev2_keyexchange *);
int	 ikev2_pld_ke(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_id(struct iked_message *, size_t, size_t,
	    struct ikev2_id *);
int	 ikev2_pld_id(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t, unsigned int);
int	 ikev2_validate_cert(struct iked_message *, size_t, size_t,
	    struct ikev2_cert *);
int	 ikev2_pld_cert(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_certreq(struct iked_message *, size_t, size_t,
	    struct ikev2_cert *);
int	 ikev2_pld_certreq(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_pld_nonce(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_notify(struct iked_message *, size_t, size_t,
	    struct ikev2_notify *);
int	 ikev2_pld_notify(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_delete(struct iked_message *, size_t, size_t,
	    struct ikev2_delete *);
int	 ikev2_pld_delete(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_ts(struct iked_message *, size_t, size_t,
	    struct ikev2_tsp *);
int	 ikev2_pld_ts(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t, unsigned int);
int	 ikev2_validate_auth(struct iked_message *, size_t, size_t,
	    struct ikev2_auth *);
int	 ikev2_pld_auth(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_pld_e(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_pld_ef(struct iked *env, struct ikev2_payload *pld,
	    struct iked_message *msg, size_t offset, size_t left);
int	 ikev2_frags_reassemble(struct iked *env,
	    struct ikev2_payload *pld, struct iked_message *msg);
int	 ikev2_validate_cp(struct iked_message *, size_t, size_t,
	    struct ikev2_cp *);
int	 ikev2_pld_cp(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);
int	 ikev2_validate_eap(struct iked_message *, size_t, size_t,
	    struct eap_header *);
int	 ikev2_pld_eap(struct iked *, struct ikev2_payload *,
	    struct iked_message *, size_t, size_t);

int
ikev2_pld_parse(struct iked *env, struct ike_header *hdr,
    struct iked_message *msg, size_t offset)
{
	log_debug("%s: header ispi %s rspi %s"
	    " nextpayload %s version 0x%02x exchange %s flags 0x%02x"
	    " msgid %d length %u response %d", __func__,
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
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);
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
    size_t offset, size_t length, unsigned int payload)
{
	struct ikev2_payload	 pld;
	unsigned int		 e;
	int			 ret;
	uint8_t			*msgbuf = ibuf_data(msg->msg_data);
	size_t			 total, left;

	/* Check if message was decrypted in an E payload */
	e = msg->msg_e ? IKED_E : 0;

	/* Bytes left in datagram. */
	total = length - offset;

	while (payload != 0 && offset < length) {
		if (ikev2_validate_pld(msg, offset, total, &pld))
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
		total -= sizeof(pld);
		left = betoh16(pld.pld_length) - sizeof(pld);
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
			ret = ikev2_pld_e(env, &pld, msg, offset, left);
			break;
		case IKEV2_PAYLOAD_SKF:
			ret = ikev2_pld_ef(env, &pld, msg, offset, left);
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

		/* Encrypted payloads must appear last */
		if ((payload == IKEV2_PAYLOAD_SK) ||
		    (payload == IKEV2_PAYLOAD_SKF))
			return (0);

		payload = pld.pld_nextpayload;
		offset += left;
		total -= left;
	}

	return (0);
}

int
ikev2_validate_sa(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_sa_proposal *sap)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);
	size_t		 sap_length;

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
	 * If there is only one proposal, sap_length must be the
	 * total payload size.
	 */
	if (!sap->sap_more && left != sap_length) {
		log_debug("%s: malformed payload: SA payload length mismatches "
		    "single proposal substructure length (%lu != %zu)",
		    __func__, left, sap_length);
		return (-1);
	}
	/*
	 * If there are more than one proposal, there must be bytes
	 * left in the payload.
	 */
	if (sap->sap_more && left <= sap_length) {
		log_debug("%s: malformed payload: SA payload too small for "
		    "further proposals (%zu <= %zu)", __func__,
		    left, sap_length);
		return (-1);
	}
	return (0);
}

int
ikev2_pld_sa(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct ikev2_sa_proposal	 sap;
	struct iked_proposal		*prop = NULL;
	uint32_t			 spi32;
	uint64_t			 spi = 0, spi64;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);
	struct iked_proposals		*props;
	size_t				 total;

	do {
		if (ikev2_validate_sa(msg, offset, left, &sap))
			return (-1);

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
			log_debug("%s: malformed payload: too long for payload "
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

		offset += total;
		left -= total;
	} while (sap.sap_more);

	return (0);
}

int
ikev2_validate_xform(struct iked_message *msg, size_t offset, size_t total,
    struct ikev2_transform *xfrm)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);
	size_t		 xfrm_length;

	if (total < sizeof(*xfrm)) {
		log_debug("%s: malformed payload: too short for header "
		    "(%zu < %zu)", __func__, total, sizeof(*xfrm));
		return (-1);
	}
	memcpy(xfrm, msgbuf + offset, sizeof(*xfrm));

	xfrm_length = betoh16(xfrm->xfrm_length);
	if (xfrm_length < sizeof(*xfrm)) {
		log_debug("%s: malformed payload: shorter than minimum header "
		    "size (%zu < %zu)", __func__, xfrm_length, sizeof(*xfrm));
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
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

	if (total < sizeof(*attr)) {
		log_debug("%s: malformed payload: too short for header "
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
	unsigned int			 type;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);
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
			log_debug("%s: malformed payload: shorter than "
			    "minimum header size (%zu < %zu)", __func__,
			    attr_length, sizeof(attr));
			return (-1);
		}
		if (total < attr_length) {
			log_debug("%s: malformed payload: attribute larger "
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
    struct ikev2_keyexchange *kex)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	uint8_t				*buf;
	size_t				 len;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_ke(msg, offset, left, &kex))
		return (-1);

	log_debug("%s: dh group %s reserved %d", __func__,
	    print_map(betoh16(kex.kex_dhgroup), ikev2_xformdh_map),
	    betoh16(kex.kex_reserved));

	buf = msgbuf + offset + sizeof(kex);
	len = left - sizeof(kex);

	if (len == 0) {
		log_debug("%s: malformed payload: no KE data given", __func__);
		return (-1);
	}

	print_hex(buf, 0, len);

	if (ikev2_msg_frompeer(msg)) {
		ibuf_release(msg->msg_parent->msg_ke);
		if ((msg->msg_parent->msg_ke = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get exchange", __func__);
			return (-1);
		}
		msg->msg_parent->msg_dhgroup = betoh16(kex.kex_dhgroup);
	}

	return (0);
}

int
ikev2_validate_id(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_id *id)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
    struct iked_message *msg, size_t offset, size_t left, unsigned int payload)
{
	uint8_t				*ptr;
	struct ikev2_id			 id;
	size_t				 len;
	struct iked_id			*idp, idb;
	struct iked_sa			*sa = msg->msg_sa;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);
	char				 idstr[IKED_ID_SIZE];

	if (ikev2_validate_id(msg, offset, left, &id))
		return (-1);

	bzero(&idb, sizeof(idb));

	/* Don't strip the Id payload header */
	ptr = msgbuf + offset;
	len = left;

	idb.id_type = id.id_type;
	idb.id_offset = sizeof(id);
	if ((idb.id_buf = ibuf_new(ptr, len)) == NULL)
		return (-1);

	if (ikev2_print_id(&idb, idstr, sizeof(idstr)) == -1) {
		ibuf_release(idb.id_buf);
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
		ibuf_release(idb.id_buf);
		log_debug("%s: unexpected id payload", __func__);
		return (0);
	}

	idp = &msg->msg_parent->msg_id;
	if (idp->id_type) {
		ibuf_release(idb.id_buf);
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
    struct ikev2_cert *cert)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	uint8_t				*buf;
	size_t				 len;
	struct iked_id			*certid;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_cert(msg, offset, left, &cert))
		return (-1);
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = left - sizeof(cert);

	log_debug("%s: type %s length %zu",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	certid = &msg->msg_parent->msg_cert;
	if (certid->id_type) {
		log_info("%s: multiple cert payloads not supported",
		   SPI_SA(msg->msg_sa, __func__));
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
    struct ikev2_cert *cert)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	struct ikev2_cert		 cert;
	struct iked_certreq		*cr;
	uint8_t				*buf;
	ssize_t				 len;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_certreq(msg, offset, left, &cert))
		return (-1);
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = left - sizeof(cert);

	log_debug("%s: type %s length %zd",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (cert.cert_type == IKEV2_CERT_X509_CERT) {
		if (len == 0) {
			log_info("%s: invalid length 0", __func__);
			return (0);
		}
		if ((len % SHA_DIGEST_LENGTH) != 0) {
			log_info("%s: invalid certificate request",
			    __func__);
			return (-1);
		}
	}

	if ((cr = calloc(1, sizeof(struct iked_certreq))) == NULL) {
		log_info("%s: failed to allocate certreq.", __func__);
		return (-1);
	}
	if ((cr->cr_data = ibuf_new(buf, len)) == NULL) {
		log_info("%s: failed to allocate buffer.", __func__);
		free(cr);
		return (-1);
	}
	cr->cr_type = cert.cert_type;
	SLIST_INSERT_HEAD(&msg->msg_parent->msg_certreqs, cr, cr_entry);

	return (0);
}

int
ikev2_validate_auth(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_auth *auth)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	uint8_t				*buf;
	size_t				 len;
	struct iked_sa			*sa = msg->msg_sa;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);

	if (ikev2_validate_auth(msg, offset, left, &auth))
		return (-1);
	offset += sizeof(auth);

	buf = msgbuf + offset;
	len = left - sizeof(auth);

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
ikev2_pld_nonce(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	size_t		 len;
	uint8_t		*buf;
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

	buf = msgbuf + offset;
	len = left;

	if (len == 0) {
		log_debug("%s: malformed payload: no NONCE given", __func__);
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
    struct ikev2_notify *n)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	uint8_t			*buf, md[SHA_DIGEST_LENGTH];
	size_t			 len;
	uint32_t		 spi32;
	uint64_t		 spi64;
	struct iked_spi		*rekey;
	uint16_t		 type;
	uint16_t		 signature_hash;

	if (ikev2_validate_notify(msg, offset, left, &n))
		return (-1);
	type = betoh16(n.n_type);

	log_debug("%s: protoid %s spisize %d type %s",
	    __func__,
	    print_map(n.n_protoid, ikev2_saproto_map), n.n_spisize,
	    print_map(type, ikev2_n_map));

	len = left - sizeof(n);
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
			log_debug("%s: %s detected NAT", __func__,
			    print_map(type, ikev2_n_map));
			if (type == IKEV2_N_NAT_DETECTION_SOURCE_IP)
				msg->msg_parent->msg_nat_detected
				    |= IKED_MSG_NAT_SRC_IP;
			else
				msg->msg_parent->msg_nat_detected
				    |= IKED_MSG_NAT_DST_IP;
		}
		print_hex(md, 0, sizeof(md));
		/* remember for MOBIKE */
		msg->msg_parent->msg_natt_rcvd = 1;
		break;
	case IKEV2_N_AUTHENTICATION_FAILED:
		if (!msg->msg_e) {
			log_debug("%s: AUTHENTICATION_FAILED not encrypted",
			    __func__);
			return (-1);
		}
		/*
		 * If we are the responder, then we only accept
		 * AUTHENTICATION_FAILED from authenticated peers.
		 * If we are the initiator, the peer cannot be authenticated.
		 */
		if (!msg->msg_sa->sa_hdr.sh_initiator) {
			if (!sa_stateok(msg->msg_sa, IKEV2_STATE_VALID)) {
				log_debug("%s: ignoring AUTHENTICATION_FAILED"
				    " from unauthenticated initiator",
				    __func__);
				return (-1);
			}
		} else {
			if (sa_stateok(msg->msg_sa, IKEV2_STATE_VALID)) {
				log_debug("%s: ignoring AUTHENTICATION_FAILED"
				    " from authenticated responder",
				    __func__);
				return (-1);
			}
		}
		msg->msg_parent->msg_flags
		    |= IKED_MSG_FLAGS_AUTHENTICATION_FAILED;
		break;
	case IKEV2_N_INVALID_KE_PAYLOAD:
		if (sa_stateok(msg->msg_sa, IKEV2_STATE_VALID) &&
		    !msg->msg_e) {
			log_debug("%s: INVALID_KE_PAYLOAD not encrypted",
			    __func__);
			return (-1);
		}
		if (len != sizeof(msg->msg_parent->msg_group)) {
			log_debug("%s: malformed payload: group size mismatch"
			    " (%zu != %zu)", __func__, len,
			    sizeof(msg->msg_parent->msg_group));
			return (-1);
		}
		memcpy(&msg->msg_parent->msg_group, buf, len);
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_INVALID_KE;
		break;
	case IKEV2_N_NO_ADDITIONAL_SAS:
		if (!msg->msg_e) {
			log_debug("%s: NO_ADDITIONAL_SAS not encrypted",
			    __func__);
			return (-1);
		}
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_NO_ADDITIONAL_SAS;
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
		if (len < sizeof(msg->msg_parent->msg_cpi) +
		    sizeof(msg->msg_parent->msg_transform)) {
			log_debug("%s: ignoring malformed ipcomp notification",
			    __func__);
			return (0);
		}
		memcpy(&msg->msg_parent->msg_cpi, buf,
		    sizeof(msg->msg_parent->msg_cpi));
		memcpy(&msg->msg_parent->msg_transform,
		    buf + sizeof(msg->msg_parent->msg_cpi),
		    sizeof(msg->msg_parent->msg_transform));

		log_debug("%s: %s cpi 0x%x, transform %s, len %zu", __func__,
		    msg->msg_parent->msg_response ? "res" : "req",
		    betoh16(msg->msg_parent->msg_cpi),
		    print_map(msg->msg_parent->msg_transform,
		    ikev2_ipcomp_map), len);

		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_IPCOMP_SUPPORTED;
		break;
	case IKEV2_N_CHILD_SA_NOT_FOUND:
		if (!msg->msg_e) {
			log_debug("%s: N_CHILD_SA_NOT_FOUND not encrypted",
			    __func__);
			return (-1);
		}
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_CHILD_SA_NOT_FOUND;
		break;
	case IKEV2_N_MOBIKE_SUPPORTED:
		if (!msg->msg_e) {
			log_debug("%s: N_MOBIKE_SUPPORTED not encrypted",
			    __func__);
			return (-1);
		}
		if (len != 0) {
			log_debug("%s: ignoring malformed mobike"
			    " notification: %zu", __func__, len);
			return (0);
		}
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_MOBIKE;
		break;
	case IKEV2_N_USE_TRANSPORT_MODE:
		if (!msg->msg_e) {
			log_debug("%s: N_USE_TRANSPORT_MODE not encrypted",
			    __func__);
			return (-1);
		}
		if (len != 0) {
			log_debug("%s: ignoring malformed transport mode"
			    " notification: %zu", __func__, len);
			return (0);
		}
		if (!(msg->msg_policy->pol_flags & IKED_POLICY_TRANSPORT)) {
			log_debug("%s: ignoring transport mode"
			    " notification (policy)", __func__);
			return (0);
		}
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_USE_TRANSPORT;
		break;
	case IKEV2_N_UPDATE_SA_ADDRESSES:
		if (!msg->msg_e) {
			log_debug("%s: N_UPDATE_SA_ADDRESSES not encrypted",
			    __func__);
			return (-1);
		}
		if (!msg->msg_sa->sa_mobike) {
			log_debug("%s: ignoring update sa addresses"
			    " notification w/o mobike: %zu", __func__, len);
			return (0);
		}
		if (len != 0) {
			log_debug("%s: ignoring malformed update sa addresses"
			    " notification: %zu", __func__, len);
			return (0);
		}
		msg->msg_parent->msg_update_sa_addresses = 1;
		break;
	case IKEV2_N_COOKIE2:
		if (!msg->msg_e) {
			log_debug("%s: N_COOKIE2 not encrypted",
			    __func__);
			return (-1);
		}
		if (!msg->msg_sa->sa_mobike) {
			log_debug("%s: ignoring cookie2 notification"
			    " w/o mobike: %zu", __func__, len);
			return (0);
		}
		if (len < IKED_COOKIE2_MIN || len > IKED_COOKIE2_MAX) {
			log_debug("%s: ignoring malformed cookie2"
			    " notification: %zu", __func__, len);
			return (0);
		}
		ibuf_release(msg->msg_cookie2);	/* should not happen */
		if ((msg->msg_cookie2 = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get peer cookie2", __func__);
			return (-1);
		}
		msg->msg_parent->msg_cookie2 = msg->msg_cookie2;
		break;
	case IKEV2_N_COOKIE:
		if (msg->msg_e) {
			log_debug("%s: N_COOKIE encrypted",
			    __func__);
			return (-1);
		}
		if (len < IKED_COOKIE_MIN || len > IKED_COOKIE_MAX) {
			log_debug("%s: ignoring malformed cookie"
			    " notification: %zu", __func__, len);
			return (0);
		}
		log_debug("%s: received cookie, len %zu", __func__, len);
		print_hex(buf, 0, len);

		ibuf_release(msg->msg_cookie);
		if ((msg->msg_cookie = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get peer cookie", __func__);
			return (-1);
		}
		msg->msg_parent->msg_cookie = msg->msg_cookie;
		break;
	case IKEV2_N_FRAGMENTATION_SUPPORTED:
		if (msg->msg_e) {
			log_debug("%s: N_FRAGMENTATION_SUPPORTED encrypted",
			    __func__);
			return (-1);
		}
		if (len != 0) {
			log_debug("%s: ignoring malformed fragmentation"
			    " notification: %zu", __func__, len);
			return (0);
		}
		msg->msg_parent->msg_flags |= IKED_MSG_FLAGS_FRAGMENTATION;
		break;
	case IKEV2_N_SIGNATURE_HASH_ALGORITHMS:
		if (msg->msg_e) {
			log_debug("%s: SIGNATURE_HASH_ALGORITHMS: encrypted",
			    __func__);
			return (-1);
		}
		if (msg->msg_sa == NULL ||
		    msg->msg_sa->sa_sigsha2) {
			log_debug("%s: SIGNATURE_HASH_ALGORITHMS: no SA or "
			    "duplicate notify", __func__);
			return (-1);
		}
		if (len < sizeof(signature_hash) ||
		    len % sizeof(signature_hash)) {
			log_debug("%s: malformed signature hash notification"
			     "(%zu bytes)", __func__, len);
			return (0);
		}
		while (len >= sizeof(signature_hash)) {
			memcpy(&signature_hash, buf, sizeof(signature_hash));
			signature_hash = betoh16(signature_hash);
			log_debug("%s: signature hash %s (%x)", __func__,
			    print_map(signature_hash, ikev2_sighash_map),
			    signature_hash);
			len -= sizeof(signature_hash);
			buf += sizeof(signature_hash);
			if (signature_hash == IKEV2_SIGHASH_SHA2_256)
				msg->msg_parent->msg_flags
				    |= IKED_MSG_FLAGS_SIGSHA2;
		}
		break;
	}

	return (0);
}

int
ikev2_validate_delete(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_delete *del)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	struct ibuf		*spibuf = NULL;
	uint64_t		*localspi = NULL;
	uint64_t		 spi64, spi = 0;
	uint32_t		 spi32;
	uint8_t			*buf, *msgbuf = ibuf_data(msg->msg_data);
	size_t			 found = 0, failed = 0;
	int			 cnt, i, len, sz, ret = -1;

	/* Skip if it's a response, then we don't have to deal with it */
	if (ikev2_msg_frompeer(msg) &&
	    msg->msg_parent->msg_response)
		return (0);

	if (ikev2_validate_delete(msg, offset, left, &del))
		return (-1);
	cnt = betoh16(del.del_nspi);
	sz = del.del_spisize;

	log_debug("%s: proto %s spisize %d nspi %d",
	    __func__, print_map(del.del_protoid, ikev2_saproto_map),
	    sz, cnt);

	buf = msgbuf + offset + sizeof(del);
	len = left - sizeof(del);

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
			ikev2_ikesa_recv_delete(env, sa);
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
	     (localspi = calloc(cnt, sizeof(uint64_t))) == NULL)) {
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
			    SPI_SA(sa, __func__),
			    print_spi(spi, del.del_spisize));
			continue;
		}

		if (ikev2_childsa_delete(env, sa, del.del_protoid, spi,
		    &localspi[i], 0) == -1)
			failed++;
		else {
			found++;

			/* append SPI to log buffer */
			if (ibuf_strlen(spibuf))
				ibuf_strcat(&spibuf, ", ");
			ibuf_strcat(&spibuf, print_spi(spi, sz));
		}

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
			if (localspi[i] == 0)	/* happens if found < cnt */
				continue;
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
		log_info("%sdeleted %zu SPI%s: %.*s",
		    SPI_SA(sa, NULL), found,
		    found == 1 ? "" : "s",
		    spibuf ? ibuf_strlen(spibuf) : 0,
		    spibuf ? (char *)ibuf_data(spibuf) : "");
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
	free(localspi);
	free(peersas);
	ibuf_release(spibuf);
	ibuf_release(resp);
	return (ret);
}

int
ikev2_validate_ts(struct iked_message *msg, size_t offset, size_t left,
    struct ikev2_tsp *tsp)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
    struct iked_message *msg, size_t offset, size_t left, unsigned int payload)
{
	struct ikev2_tsp		 tsp;
	struct ikev2_ts			 ts;
	size_t				 len, i;
	struct sockaddr_in		 s4;
	struct sockaddr_in6		 s6;
	uint8_t				 buf[2][128];
	uint8_t				*ptr;

	if (ikev2_validate_ts(msg, offset, left, &tsp))
		return (-1);

	ptr = ibuf_data(msg->msg_data) + offset;
	len = left;

	ptr += sizeof(tsp);
	len -= sizeof(tsp);

	log_debug("%s: count %d length %zu", __func__,
	    tsp.tsp_count, len);

	for (i = 0; i < tsp.tsp_count; i++) {
		if (len < sizeof(ts)) {
			log_debug("%s: malformed payload: too short for ts "
			    "(%zu < %zu)", __func__, left, sizeof(ts));
			return (-1);
		}
		memcpy(&ts, ptr, sizeof(ts));
		/* Note that ts_length includes header sizeof(ts) */
		if (len < betoh16(ts.ts_length)) {
			log_debug("%s: malformed payload: too short for "
			    "ts_length (%zu < %u)", __func__, len,
			    betoh16(ts.ts_length));
			return (-1);
		}

		log_debug("%s: type %s protoid %u length %d "
		    "startport %u endport %u", __func__,
		    print_map(ts.ts_type, ikev2_ts_map),
		    ts.ts_protoid, betoh16(ts.ts_length),
		    betoh16(ts.ts_startport),
		    betoh16(ts.ts_endport));

		switch (ts.ts_type) {
		case IKEV2_TS_IPV4_ADDR_RANGE:
			if (betoh16(ts.ts_length) < sizeof(ts) + 2 * 4) {
				log_debug("%s: malformed payload: too short "
				    "for ipv4 addr range (%u < %u)",
				    __func__, betoh16(ts.ts_length), 2 * 4);
				return (-1);
			}
			bzero(&s4, sizeof(s4));
			s4.sin_family = AF_INET;
			s4.sin_len = sizeof(s4);
			memcpy(&s4.sin_addr.s_addr, ptr + sizeof(ts), 4);
			print_host((struct sockaddr *)&s4,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s4.sin_addr.s_addr, ptr + sizeof(ts) + 4, 4);
			print_host((struct sockaddr *)&s4,
			    (char *)buf[1], sizeof(buf[1]));
			log_debug("%s: start %s end %s", __func__,
			    buf[0], buf[1]);
			break;
		case IKEV2_TS_IPV6_ADDR_RANGE:
			if (betoh16(ts.ts_length) < sizeof(ts) + 2 * 16) {
				log_debug("%s: malformed payload: too short "
				    "for ipv6 addr range (%u < %u)",
				    __func__, betoh16(ts.ts_length), 2 * 16);
				return (-1);
			}
			bzero(&s6, sizeof(s6));
			s6.sin6_family = AF_INET6;
			s6.sin6_len = sizeof(s6);
			memcpy(&s6.sin6_addr, ptr + sizeof(ts), 16);
			print_host((struct sockaddr *)&s6,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s6.sin6_addr, ptr + sizeof(ts) + 16, 16);
			print_host((struct sockaddr *)&s6,
			    (char *)buf[1], sizeof(buf[1]));
			log_debug("%s: start %s end %s", __func__,
			    buf[0], buf[1]);
			break;
		default:
			break;
		}

		ptr += betoh16(ts.ts_length);
		len -= betoh16(ts.ts_length);
	}

	return (0);
}

int
ikev2_pld_ef(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct iked_sa			*sa = msg->msg_sa;
	struct iked_frag		*sa_frag = &sa->sa_fragments;
	struct iked_frag_entry		*el;
	struct ikev2_frag_payload	 frag;
	uint8_t				*msgbuf = ibuf_data(msg->msg_data);
	uint8_t				*buf;
	struct ibuf			*e = NULL;
	size_t				 frag_num, frag_total;
	size_t			 	 len;
	int				 ret = -1;
	ssize_t				 elen;

	buf = msgbuf + offset;
	memcpy(&frag, buf, sizeof(frag));
	frag_num = betoh16(frag.frag_num);
	frag_total = betoh16(frag.frag_total);

	offset += sizeof(frag);
	buf = msgbuf + offset;
	len = left - sizeof(frag);

	/* Limit number of total fragments to avoid DOS */
	if (frag_total > IKED_FRAG_TOTAL_MAX ) {
		log_debug("%s: Total Fragments too big  %zu",
		    __func__, frag_total);
		goto dropall;
	}

	/* Check sanity of fragment header */
	if (frag_num == 0 || frag_total == 0) {
		log_debug("%s: Malformed fragment received: %zu of %zu",
		    __func__, frag_num, frag_total);
		goto done;
	}
	log_debug("%s: Received fragment: %zu of %zu",
	     __func__, frag_num, frag_total);

	/* Check new fragmented message */
	if (sa_frag->frag_arr == NULL) {
		sa_frag->frag_arr = recallocarray(NULL, 0, frag_total,
		    sizeof(struct iked_frag_entry*));
		if (sa_frag->frag_arr == NULL) {
			log_info("%s: recallocarray sa_frag->frag_arr.", __func__);
			goto done;
		}
		sa_frag->frag_total = frag_total;
		sa_frag->frag_nextpayload = pld->pld_nextpayload;
	}

	/* Drop all fragments if frag_num or frag_total don't match */
	if (frag_num > sa_frag->frag_total || frag_total != sa_frag->frag_total)
		goto dropall;

	/* Silent drop if fragment already stored */
	if (sa_frag->frag_arr[frag_num-1] != NULL)
		goto done;

        /* Decrypt fragment */
	if ((e = ibuf_new(buf, len)) == NULL)
		goto done;

	if ((e = ikev2_msg_decrypt(env, msg->msg_sa, msg->msg_data, e))
	    == NULL ) {
		log_debug("%s: Failed to decrypt fragment: %zu of %zu",
		    __func__, frag_num, frag_total);
		goto done;
	}
	elen = ibuf_length(e);

	/* Insert new list element */
	el = calloc(1, sizeof(struct iked_frag_entry));
	if (el == NULL) {
		log_info("%s: Failed allocating new fragment: %zu of %zu",
		    __func__, frag_num, frag_total);
		goto done;
	}

	sa_frag->frag_arr[frag_num-1] = el;
	el->frag_size = elen;
	el->frag_data = calloc(1, elen);
	if (el->frag_data == NULL) {
		log_debug("%s: Failed allocating new fragment data: %zu of %zu",
		    __func__, frag_num, frag_total);
		goto done;
	}

	/* Copy plaintext to fragment */
	memcpy(el->frag_data, ibuf_seek(e, 0, 0), elen);
	sa_frag->frag_total_size += elen;
	sa_frag->frag_count++;

	/* If all frags are received start reassembly */
	if (sa_frag->frag_count == sa_frag->frag_total) {
		log_debug("%s: All fragments received: %zu of %zu",
		    __func__, frag_num, frag_total);
		ret = ikev2_frags_reassemble(env, pld, msg);
	} else {
		ret = 0;
	}
done:
	ibuf_release(e);
	return (ret);
dropall:
	config_free_fragments(sa_frag);
	ibuf_release(e);
	return -1;
}

int
ikev2_frags_reassemble(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg)
{
	struct iked_frag		*sa_frag = &msg->msg_sa->sa_fragments;
	struct ibuf			*e = NULL;
	struct iked_frag_entry		*el;
	size_t				 offset;
	size_t				 i;
	struct iked_message		 emsg;
	int				 ret = -1;

	/* Reassemble fragments to single buffer */
	if ((e = ibuf_new(NULL, sa_frag->frag_total_size)) == NULL) {
		log_debug("%s: Failed allocating SK buffer.", __func__);
		goto done;
	}

	/* Empty queue to new buffer */
	offset = 0;
	for (i = 0; i < sa_frag->frag_total; i++) {
		if ((el = sa_frag->frag_arr[i]) == NULL)
			fatalx("Tried to reassemble shallow frag_arr");
		memcpy(ibuf_seek(e, offset, 0), el->frag_data, el->frag_size);
		offset += el->frag_size;
	}

	log_debug("%s: Defragmented length %zd", __func__,
	    sa_frag->frag_total_size);
	print_hex(ibuf_data(e), 0,  sa_frag->frag_total_size);

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
	    sa_frag->frag_nextpayload);
done:
	config_free_fragments(sa_frag);
	ibuf_release(e);

	return (ret);
}

int
ikev2_pld_e(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, size_t offset, size_t left)
{
	struct iked_sa		*sa = msg->msg_sa;
	struct ibuf		*e = NULL;
	uint8_t			*msgbuf = ibuf_data(msg->msg_data);
	struct iked_message	 emsg;
	uint8_t			*buf;
	size_t			 len;
	int			 ret = -1;

	if (sa->sa_fragments.frag_arr != NULL) {
		log_warn("%s: Received SK payload when SKFs are in queue.",
		    __func__);
		config_free_fragments(&sa->sa_fragments);
		return (ret);
	}

	buf = msgbuf + offset;
	len = left;

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
    struct ikev2_cp *cp)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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
	uint8_t			*ptr;
	size_t			 len;
	struct iked_sa		*sa = msg->msg_sa;

	if (ikev2_validate_cp(msg, offset, left, &cp))
		return (-1);

	ptr = ibuf_data(msg->msg_data) + offset + sizeof(cp);
	len = left - sizeof(cp);

	log_debug("%s: type %s length %zu",
	    __func__, print_map(cp.cp_type, ikev2_cp_map), len);
	print_hex(ptr, 0, len);

	while (len > 0) {
		if (len < sizeof(*cfg)) {
			log_debug("%s: malformed payload: too short for cfg "
			    "(%zu < %zu)", __func__, len, sizeof(*cfg));
			return (-1);
		}
		cfg = (struct ikev2_cfg *)ptr;

		log_debug("%s: %s 0x%04x length %d", __func__,
		    print_map(betoh16(cfg->cfg_type), ikev2_cfg_map),
		    betoh16(cfg->cfg_type),
		    betoh16(cfg->cfg_length));

		ptr += sizeof(*cfg);
		len -= sizeof(*cfg);

		if (len < betoh16(cfg->cfg_length)) {
			log_debug("%s: malformed payload: too short for "
			    "cfg_length (%zu < %u)", __func__, len,
			    betoh16(cfg->cfg_length));
			return (-1);
		}

		ptr += betoh16(cfg->cfg_length);
		len -= betoh16(cfg->cfg_length);
	}

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (sa)
		sa->sa_cp = cp.cp_type;

	return (0);
}

int
ikev2_validate_eap(struct iked_message *msg, size_t offset, size_t left,
    struct eap_header *hdr)
{
	uint8_t		*msgbuf = ibuf_data(msg->msg_data);

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

	if (ikev2_validate_eap(msg, offset, left, &hdr))
		return (-1);
	len = betoh16(hdr.eap_length);

	if (len < sizeof(*eap)) {
		log_info("%s: %s id %d length %d", SPI_SA(sa, __func__),
		    print_map(hdr.eap_code, eap_code_map),
		    hdr.eap_id, betoh16(hdr.eap_length));
	} else {
		/* Now try to get the indicated length */
		if ((eap = ibuf_seek(msg->msg_data, offset, len)) == NULL) {
			log_debug("%s: invalid EAP length", __func__);
			return (-1);
		}

		log_info("%s: %s id %d length %d EAP-%s", SPI_SA(sa, __func__),
		    print_map(eap->eap_code, eap_code_map),
		    eap->eap_id, betoh16(eap->eap_length),
		    print_map(eap->eap_type, eap_type_map));

		if (eap_parse(env, sa, eap, msg->msg_response) == -1)
			return (-1);
	}

	return (0);
}

/*	$OpenBSD: ikev2_pld.c,v 1.2 2010/06/14 08:10:32 reyk Exp $	*/
/*	$vantronix: ikev2.c,v 1.101 2010/06/03 07:57:33 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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
#include <sys/types.h>
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

int	 ikev2_pld_payloads(struct iked *, struct iked_message *,
	    off_t, size_t, u_int, int);
int	 ikev2_pld_sa(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_xform(struct iked *, struct ikev2_sa_proposal *,
	    struct iked_message *, off_t);
int	 ikev2_pld_attr(struct iked *, struct ikev2_transform *,
	    struct iked_message *, off_t, int);
int	 ikev2_pld_ke(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_id(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t, u_int);
int	 ikev2_pld_cert(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_certreq(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_nonce(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_notify(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_delete(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_ts(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t, u_int);
int	 ikev2_pld_auth(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_e(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_cp(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_pld_eap(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);

int
ikev2_pld_parse(struct iked *env, struct ike_header *hdr,
    struct iked_message *msg, off_t offset)
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
	    betoh32(hdr->ike_length), hdr->ike_nextpayload, 0));
}

int
ikev2_pld_payloads(struct iked *env, struct iked_message *msg,
    off_t offset, size_t length, u_int payload, int quick)
{
	struct ikev2_payload	 pld;
	u_int			 e;
	int			 ret;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);

	/* Check if message was decrypted in an E payload */
	e = msg->msg_decrypted ? IKED_E : 0;

	if (quick)
		print_debug("%s: %spayloads", __func__,
		    e ? "decrypted " : "");
	else
		ikev2_pld_payloads(env, msg, offset, length, payload, 1);

	while (payload != 0 && offset < (off_t)length) {
		memcpy(&pld, msgbuf + offset, sizeof(pld));

		if (quick)
			print_debug(" %s",
			    print_map(payload, ikev2_payload_map));
		else
			log_debug("%s: %spayload %s"
			    " nextpayload %s critical 0x%02x length %d",
			    __func__, e ? "decrypted " : "",
			    print_map(payload, ikev2_payload_map),
			    print_map(pld.pld_nextpayload, ikev2_payload_map),
			    pld.pld_reserved & IKEV2_CRITICAL_PAYLOAD,
			    betoh16(pld.pld_length));

		offset += sizeof(pld);
		ret = 0;

		if (quick)
			goto next;

		switch (payload | e) {
		case IKEV2_PAYLOAD_SA:
		case IKEV2_PAYLOAD_SA | IKED_E:
			ret = ikev2_pld_sa(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_KE:
			ret = ikev2_pld_ke(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_IDi | IKED_E:
		case IKEV2_PAYLOAD_IDr | IKED_E:
			ret = ikev2_pld_id(env, &pld, msg, offset, payload);
			break;
		case IKEV2_PAYLOAD_CERT | IKED_E:
			ret = ikev2_pld_cert(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_CERTREQ:
		case IKEV2_PAYLOAD_CERTREQ | IKED_E:
			ret = ikev2_pld_certreq(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_AUTH | IKED_E:
			ret = ikev2_pld_auth(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_NONCE:
		case IKEV2_PAYLOAD_NONCE | IKED_E:
			ret = ikev2_pld_nonce(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_NOTIFY:
		case IKEV2_PAYLOAD_NOTIFY | IKED_E:
			ret = ikev2_pld_notify(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_DELETE | IKED_E:
			ret = ikev2_pld_delete(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_TSi | IKED_E:
		case IKEV2_PAYLOAD_TSr | IKED_E:
			ret = ikev2_pld_ts(env, &pld, msg, offset, payload);
			break;
		case IKEV2_PAYLOAD_E:
			ret = ikev2_pld_e(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_CP | IKED_E:
			ret = ikev2_pld_cp(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_EAP | IKED_E:
			ret = ikev2_pld_eap(env, &pld, msg, offset);
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
		if (payload == IKEV2_PAYLOAD_E)
			return (0);

 next:
		payload = pld.pld_nextpayload;
		offset += betoh16(pld.pld_length) - sizeof(pld);
	}

	if (quick)
		print_debug("\n");

	return (0);
}

int
ikev2_pld_sa(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_sa_proposal	 sap;
	struct iked_proposal		*prop = NULL;
	u_int32_t			 spi32;
	u_int64_t			 spi = 0, spi64;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa			*sa = msg->msg_sa;

	memcpy(&sap, msgbuf + offset, sizeof(sap));
	offset += sizeof(sap);

	if (sap.sap_spisize) {
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
	}

	log_debug("%s: more %d reserved %d length %d"
	    " proposal #%d protoid %s spisize %d xforms %d spi %s",
	    __func__, sap.sap_more, sap.sap_reserved,
	    betoh16(sap.sap_length), sap.sap_proposalnr,
	    print_map(sap.sap_protoid, ikev2_saproto_map), sap.sap_spisize,
	    sap.sap_transforms, print_spi(spi, sap.sap_spisize));

	if (ikev2_msg_frompeer(msg)) {
		if ((msg->msg_prop = config_add_proposal(&msg->msg_proposals,
		    sap.sap_proposalnr, sap.sap_protoid)) == NULL) {
			log_debug("%s: invalid proposal", __func__);
			return (-1);
		}
		prop = msg->msg_prop;
		prop->prop_localspi.spi_size = sap.sap_spisize;
		prop->prop_peerspi.spi = spi;
	}

	/*
	 * Parse the attached transforms
	 */
	if (ikev2_pld_xform(env, &sap, msg, offset) != 0) {
		log_debug("%s: invalid proposal transforms", __func__);
		return (-1);
	}

	if (!ikev2_msg_frompeer(msg))
		return (0);

	/* XXX we need a better way to get this */
	if (ikev2_sa_negotiate(sa,
	    &msg->msg_policy->pol_proposals,
	    &msg->msg_proposals, msg->msg_decrypted ?
	    IKEV2_SAPROTO_ESP : IKEV2_SAPROTO_IKE) != 0) {
		log_debug("%s: no proposal chosen", __func__);
		msg->msg_error = IKEV2_N_NO_PROPOSAL_CHOSEN;
		return (-1);
	} else if (sa_stateok(sa, IKEV2_STATE_SA_INIT))
		sa_stateflags(sa, IKED_REQ_SA);

	return (0);
}

int
ikev2_pld_xform(struct iked *env, struct ikev2_sa_proposal *sap,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_transform		 xfrm;
	char				 id[BUFSIZ];
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&xfrm, msgbuf + offset, sizeof(xfrm));

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

	log_debug("%s: more %d reserved %d length %d"
	    " type %s id %s",
	    __func__, xfrm.xfrm_more, xfrm.xfrm_reserved,
	    betoh16(xfrm.xfrm_length),
	    print_map(xfrm.xfrm_type, ikev2_xformtype_map), id);

	/*
	 * Parse transform attributes, if available
	 */
	msg->msg_attrlength = 0;
	if ((u_int)betoh16(xfrm.xfrm_length) > sizeof(xfrm))
		ikev2_pld_attr(env, &xfrm, msg, offset + sizeof(xfrm),
		    betoh16(xfrm.xfrm_length) - sizeof(xfrm));

	if (ikev2_msg_frompeer(msg)) {
		if (config_add_transform(msg->msg_prop, xfrm.xfrm_type,
		    betoh16(xfrm.xfrm_id), msg->msg_attrlength,
		    msg->msg_attrlength) == NULL) {
			log_debug("%s: failed to add transform", __func__);
			return (-1);
		}
	}

	/* Next transform */
	offset += betoh16(xfrm.xfrm_length);
	if (xfrm.xfrm_more == IKEV2_XFORM_MORE)
		ikev2_pld_xform(env, sap, msg, offset);

	return (0);
}

int
ikev2_pld_attr(struct iked *env, struct ikev2_transform *xfrm,
    struct iked_message *msg, off_t offset, int total)
{
	struct ikev2_attribute		 attr;
	u_int				 type;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&attr, msgbuf + offset, sizeof(attr));

	type = betoh16(attr.attr_type) & ~IKEV2_ATTRAF_TV;

	log_debug("%s: attribute type %s length %d total %d",
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
		print_hex(msgbuf, offset + sizeof(attr),
		    betoh16(attr.attr_length) - sizeof(attr));
		offset += betoh16(attr.attr_length);
		total -= betoh16(attr.attr_length);
	}

	if (total > 0) {
		/* Next attribute */
		ikev2_pld_attr(env, xfrm, msg, offset, total);
	}

	return (0);
}

int
ikev2_pld_ke(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_keyexchange	 kex;
	u_int8_t			*buf;
	size_t				 len;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&kex, msgbuf + offset, sizeof(kex));

	log_debug("%s: dh group %s reserved %d",
	    __func__,
	    print_map(betoh16(kex.kex_dhgroup), ikev2_xformdh_map),
	    betoh16(kex.kex_reserved));

	buf = msgbuf + offset + sizeof(kex);
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(kex);

	print_hex(buf, 0, len);

	if (ikev2_msg_frompeer(msg)) {
		if ((msg->msg_sa->sa_dhiexchange =
		    ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get exchange", __func__);
			return (-1);
		}
	}

	return (0);
}

int
ikev2_pld_id(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset, u_int payload)
{
	u_int8_t			*ptr;
	struct ikev2_id			 id;
	size_t				 len;
	struct iked_id			*idp, idb;
	struct iked_sa			*sa = msg->msg_sa;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);
	struct ibuf			*authmsg;
	char				 idstr[IKED_ID_SIZE];

	memcpy(&id, msgbuf + offset, sizeof(id));
	bzero(&idb, sizeof(idb));

	/* Don't strip the Id payload header */
	ptr = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);

	idb.id_type = id.id_type;
	if ((idb.id_buf = ibuf_new(ptr, len)) == NULL)
		return (-1);

	if (print_id(&idb, sizeof(id), idstr, sizeof(idstr)) == -1) {
		log_debug("%s: malformed id", __func__);
		return (-1);
	}

	log_debug("%s: id %s/%s length %d",
	    __func__, print_map(id.id_type, ikev2_id_map), idstr, len);

	if (!ikev2_msg_frompeer(msg)) {
		ibuf_release(idb.id_buf);
		return (0);
	}

	if (sa->sa_hdr.sh_initiator && payload == IKEV2_PAYLOAD_IDr) {
		idp = &sa->sa_rid;
	} else if (!sa->sa_hdr.sh_initiator && payload == IKEV2_PAYLOAD_IDi) {
		idp = &sa->sa_iid;
	} else {
		log_debug("%s: unexpected id payload", __func__);
		return (0);
	}

	ibuf_release(idp->id_buf);
	idp->id_buf = idb.id_buf;
	idp->id_type = idb.id_type;

	if ((authmsg = ikev2_msg_auth(env, sa,
	    !sa->sa_hdr.sh_initiator)) == NULL) {
		log_debug("%s: failed to get response auth data", __func__);
		return (-1);
	}

	ca_setauth(env, sa, authmsg, PROC_CERT);

	return (0);
}

int
ikev2_pld_cert(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_cert		 cert;
	u_int8_t			*buf;
	size_t				 len;
	struct iked_sa			*sa = msg->msg_sa;
	struct iked_id			*certid, *id;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&cert, msgbuf + offset, sizeof(cert));
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cert);

	log_debug("%s: type %s length %d",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (!sa->sa_hdr.sh_initiator && !msg->msg_response) {
		certid = &sa->sa_icert;
		id = &sa->sa_iid;
	} else if (sa->sa_hdr.sh_initiator && msg->msg_response) {
		certid = &sa->sa_rcert;
		id = &sa->sa_rid;
	} else
		return (0);	/* ignore */

	if ((certid->id_buf = ibuf_new(buf, len)) == NULL) {
		log_debug("%s: failed to save cert", __func__);
		return (-1);
	}
	certid->id_type = cert.cert_type;

	ca_setcert(env, &msg->msg_sa->sa_hdr, id, cert.cert_type,
	    buf, len, PROC_CERT);

	return (0);
}

int
ikev2_pld_certreq(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_cert		 cert;
	u_int8_t			*buf;
	size_t				 len;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&cert, msgbuf + offset, sizeof(cert));
	offset += sizeof(cert);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cert);

	log_debug("%s: type %s signatures length %d",
	    __func__, print_map(cert.cert_type, ikev2_cert_map), len);
	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if (!len || (len % SHA_DIGEST_LENGTH) != 0) {
		log_debug("%s: invalid certificate request", __func__);
		return (-1);
	}

	if (msg->msg_sa == NULL)
		return (-1);

	/* Optional certreq for PSK */
	msg->msg_sa->sa_staterequire |= IKED_REQ_CERT;

	ca_setreq(env, &msg->msg_sa->sa_hdr, cert.cert_type,
	    buf, len, PROC_CERT);

	return (0);
}

int
ikev2_pld_auth(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_auth		 auth;
	struct iked_auth		 ikeauth;
	u_int8_t			*buf;
	size_t				 len;
	struct ibuf			*authmsg;
	struct iked_sa			*sa = msg->msg_sa;
	struct iked_policy		*policy = sa->sa_policy;
	int				 ret = -1;
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&auth, msgbuf + offset, sizeof(auth));
	offset += sizeof(auth);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(auth);

	log_debug("%s: method %s length %d",
	    __func__, print_map(auth.auth_method, ikev2_auth_map), len);

	print_hex(buf, 0, len);

	if (!ikev2_msg_frompeer(msg))
		return (0);

	memcpy(&ikeauth, &policy->pol_auth, sizeof(ikeauth));

	if (policy->pol_auth.auth_eap && sa->sa_eapmsk != NULL) {
		/* The initiator EAP auth is a PSK derived from the MSK */
		ikeauth.auth_method = IKEV2_AUTH_SHARED_KEY_MIC;

		/* Copy session key as PSK */
		memcpy(ikeauth.auth_data, ibuf_data(sa->sa_eapmsk),
		    ibuf_size(sa->sa_eapmsk));
		ikeauth.auth_length = ibuf_size(sa->sa_eapmsk);
	}
	if (auth.auth_method != ikeauth.auth_method) {
		log_debug("%s: method %s required", __func__,
		    print_map(ikeauth.auth_method, ikev2_auth_map));
		return (-1);
	}

	/* The AUTH payload indicates if the responder wants EAP or not */
	if (!sa_stateok(sa, IKEV2_STATE_EAP))
		sa_state(env, sa, IKEV2_STATE_AUTH_REQUEST);

	if ((authmsg = ikev2_msg_auth(env, sa,
	    sa->sa_hdr.sh_initiator)) == NULL) {
		log_debug("%s: failed to get auth data", __func__);
		return (-1);
	}

	ret = ikev2_msg_authverify(env, sa, &ikeauth, buf, len,
	    authmsg);

	ibuf_release(authmsg);
	authmsg = NULL;

	if (ret != 0)
		goto done;

	if (sa->sa_eapmsk != NULL) {
		if ((authmsg = ikev2_msg_auth(env, sa,
		    !sa->sa_hdr.sh_initiator)) == NULL) {
			log_debug("%s: failed to get auth data", __func__);
			return (-1);
		}

		/* 2nd AUTH for EAP messages */
		if ((ret = ikev2_msg_authsign(env, sa,
		    &ikeauth, authmsg)) != 0)
			goto done;

		sa_state(env, sa, IKEV2_STATE_EAP_VALID);
	}

 done:
	ibuf_release(authmsg);

	return (ret);
}

int
ikev2_pld_nonce(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	size_t		 len;
	u_int8_t	*buf;
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa	*sa = msg->msg_sa;
	struct ibuf	*peernonce;

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);
	print_hex(buf, 0, len);

	if (ikev2_msg_frompeer(msg)) {
		if ((peernonce = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get peer nonce", __func__);
			return (-1);
		}

		log_debug("%s: updating peer nonce", __func__);

		if (sa->sa_hdr.sh_initiator) {
			ibuf_release(sa->sa_rnonce);
			sa->sa_rnonce = peernonce;
		} else {
			ibuf_release(sa->sa_inonce);
			sa->sa_inonce = peernonce;
		}
	}

	return (0);
}

int
ikev2_pld_notify(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_notify	*n;
	u_int8_t		*buf, md[SHA_DIGEST_LENGTH];
	size_t			 len;
	u_int16_t		 type;
	u_int32_t		 spi32;
	u_int64_t		 spi64;
	struct iked_spi		*rekey;
	
	if ((n = ibuf_seek(msg->msg_data, offset, sizeof(*n))) == NULL)
		return (-1);
	type = betoh16(n->n_type);

	log_debug("%s: protoid %s spisize %d type %s",
	    __func__,
	    print_map(n->n_protoid, ikev2_saproto_map), n->n_spisize,
	    print_map(type, ikev2_n_map));

	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(*n);
	if ((buf = ibuf_seek(msg->msg_data, offset + sizeof(*n), len)) == NULL)
		return (-1);

	print_hex(buf, 0, len);

	switch (type) {
	case IKEV2_N_NAT_DETECTION_SOURCE_IP:
	case IKEV2_N_NAT_DETECTION_DESTINATION_IP:
		if (ikev2_nat_detection(msg, md, sizeof(md), type,
		    msg->msg_response) == -1)
			return (-1);
		if (len != sizeof(md) || memcmp(buf, md, len) != 0) {
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
	case IKEV2_N_REKEY_SA:
		if (len != n->n_spisize) {
			log_debug("%s: malformed notification", __func__);
			return (-1);
		}
		if (msg->msg_decrypted)
			rekey = &msg->msg_decrypted->msg_rekey;
		else
			rekey = &msg->msg_rekey;
		if (rekey->spi != 0) {
			log_debug("%s: rekeying of multiple SAs not supported",
			    __func__);
			return (-1);
		}
		switch (n->n_spisize) {
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
			    n->n_spisize);
			return (-1);
		}
		rekey->spi_size = n->n_spisize;
		rekey->spi_protoid = n->n_protoid;

		log_debug("%s: rekey %s spi %s", __func__,
		    print_map(n->n_protoid, ikev2_saproto_map),
		    print_spi(rekey->spi, n->n_spisize));
		break;
	}

	return (0);
}

int
ikev2_pld_delete(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_delete	*del, *localdel;
	u_int64_t		 spi64, spi = 0, *localspi = NULL;
	u_int32_t		 spi32;
	size_t			 len, i, cnt, sz, found = 0, failed = 0;
	u_int8_t		*buf, firstpayload = 0;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa		*sa = msg->msg_sa;
	struct ibuf		*resp = NULL;
	int			 ret = -1;

	if ((del = ibuf_seek(msg->msg_data, offset, sizeof(*del))) == NULL)
		return (-1);
	cnt = betoh16(del->del_nspi);
	sz = del->del_spisize;

	log_debug("%s: protoid %s spisize %d nspi %d",
	    __func__, print_map(del->del_protoid, ikev2_saproto_map),
	    sz, cnt);

	buf = msgbuf + offset + sizeof(*del);
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(*del);

	print_hex(buf, 0, len);

	switch (sz) {
	case 4:
	case 8:
		break;
	default:
		if (ikev2_msg_frompeer(msg) &&
		    del->del_protoid == IKEV2_SAPROTO_IKE) {
			sa_state(env, sa, IKEV2_STATE_DELETE);
			return (0);
		}
		log_debug("%s: invalid SPI size", __func__);
		return (-1);
	}

	if ((len / sz) != cnt) {
		log_debug("%s: invalid payload length %d/%d != %d",
		    __func__, len, sz, cnt);
		return (-1);
	}

	if (ikev2_msg_frompeer(msg) &&
	    (localspi = calloc(cnt, sizeof(u_int64_t))) == NULL) {
		log_warn("%s", __func__);
		return (-1);
	}

	for (i = 0; i < cnt; i++) {
		/* XXX delete SAs */
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
		if (!ikev2_msg_frompeer(msg)) {
			log_debug("%s: spi %s", __func__, print_spi(spi, sz));
			continue;
		}

		if (ikev2_childsa_delete(env, sa,
		    del->del_protoid, spi, &localspi[i], 0) == -1)
			failed++;
		else
			found++;
	}

	if (!ikev2_msg_frompeer(msg))
		return (0);

	if ((resp = ibuf_static()) == NULL)
		goto done;

	if (found) {
		if ((localdel = ibuf_advance(resp, sizeof(*localdel))) == NULL)
			goto done;

		firstpayload = IKEV2_PAYLOAD_DELETE;
		localdel->del_protoid = del->del_protoid;
		localdel->del_spisize = del->del_spisize;
		localdel->del_nspi = htobe16(found);

		for (i = 0; i < cnt; i++) {
			if (!localspi[i])
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

	}

	if (found) {
		ret = ikev2_send_ike_e(env, sa, resp,
		    firstpayload, IKEV2_EXCHANGE_INFORMATIONAL, 1);
	} else {
		/* XXX should we send an INVALID_SPI notification? */
		ret = 0;
	}

 done:
	if (localspi != NULL)
		free(localspi);
	ibuf_release(resp);
	return (ret);
}

int
ikev2_pld_ts(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset, u_int payload)
{
	u_int8_t			*ptr;
	struct ikev2_tsp		 tsp;
	struct ikev2_ts			 ts;
	size_t				 len, i;
	struct sockaddr_in		 s4;
	struct sockaddr_in6		 s6;
	u_int8_t			 buf[2][128];
	u_int8_t			*msgbuf = ibuf_data(msg->msg_data);

	memcpy(&tsp, msgbuf + offset, sizeof(tsp));
	offset += sizeof(tsp);

	ptr = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(tsp);

	log_debug("%s: count %d length %d", __func__,
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
			print_host((struct sockaddr_storage *)&s4,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s4.sin_addr.s_addr,
			    msgbuf + offset + sizeof(ts) + 4, 4);
			print_host((struct sockaddr_storage *)&s4,
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
			print_host((struct sockaddr_storage *)&s6,
			    (char *)buf[0], sizeof(buf[0]));
			memcpy(&s6.sin6_addr,
			    msgbuf + offset + sizeof(ts) + 16, 16);
			print_host((struct sockaddr_storage *)&s6,
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
    struct iked_message *msg, off_t offset)
{
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

	if ((e = ikev2_msg_decrypt(env, msg->msg_sa,
	    msg->msg_data, e)) == NULL)
		goto done;

	/*
	 * Parse decrypted payload
	 */
	bzero(&emsg, sizeof(emsg));
	memcpy(&emsg, msg, sizeof(*msg));
	emsg.msg_data = e;
	emsg.msg_decrypted = msg;
	TAILQ_INIT(&emsg.msg_proposals);

	ret = ikev2_pld_payloads(env, &emsg, 0, ibuf_size(e),
	    pld->pld_nextpayload, 0);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_pld_cp(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct ikev2_cp		 cp;
	struct ikev2_cfg	*cfg;
	u_int8_t		*buf;
	size_t			 len, i;
	u_int8_t		*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa		*sa = msg->msg_sa;

	memcpy(&cp, msgbuf + offset, sizeof(cp));
	offset += sizeof(cp);

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld) - sizeof(cp);

	log_debug("%s: type %s",
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
ikev2_pld_eap(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	struct eap_header		*hdr;
	struct eap_message		*eap = NULL;
	struct iked_sa			*sa = msg->msg_sa;
	size_t				 len;

	if ((hdr = ibuf_seek(msg->msg_data, offset, sizeof(*hdr))) == NULL) {
		log_debug("%s: failed to get EAP header", __func__);
		return (-1);
	}

	len = betoh16(hdr->eap_length);

	if (len < sizeof(*eap)) {
		log_info("%s: %s id %d length %d", __func__,
		    print_map(hdr->eap_code, eap_code_map),
		    hdr->eap_id, betoh16(hdr->eap_length));
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
	}

	if (eap_parse(env, sa, hdr, msg->msg_response) == -1)
		return (-1);

	return (0);
}

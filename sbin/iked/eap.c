/*	$OpenBSD: eap.c,v 1.11 2015/01/16 06:39:58 deraadt Exp $	*/

/*
 * Copyright (c) 2010-2013 Reyk Floeter <reyk@openbsd.org>
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
#include "chap_ms.h"

char	*eap_identity_response(struct eap_message *);
int	 eap_challenge_request(struct iked *env, struct iked_sa *,
	    struct eap_header *);
int	 eap_success(struct iked *, struct iked_sa *, struct eap_header *);
int	 eap_mschap(struct iked *, struct iked_sa *, struct eap_message *);

ssize_t
eap_identity_request(struct ibuf *e)
{
	struct eap_message		*eap;

	if ((eap = ibuf_advance(e, sizeof(*eap))) == NULL)
		return (-1);
	eap->eap_code = EAP_CODE_REQUEST;
	eap->eap_id = 0;
	eap->eap_length = htobe16(sizeof(*eap));
	eap->eap_type = EAP_TYPE_IDENTITY;

	return (sizeof(*eap));
}

char *
eap_identity_response(struct eap_message *eap)
{
	size_t				 len;
	char				*str;
	u_int8_t			*ptr = (u_int8_t *)eap;

	len = betoh16(eap->eap_length) - sizeof(*eap);
	ptr += sizeof(*eap);

	if (len == 0 || (str = get_string(ptr, len)) == NULL) {
		log_info("%s: invalid identity response, length %zu",
		    __func__, len);
		return (NULL);
	}
	log_debug("%s: identity '%s' length %zd", __func__, str, len);
	return (str);
}

int
eap_challenge_request(struct iked *env, struct iked_sa *sa,
    struct eap_header *hdr)
{
	struct eap_message		*eap;
	struct eap_mschap_challenge	*ms;
	const char			*name;
	int				 ret = -1;
	struct ibuf			*e;

	if ((e = ibuf_static()) == NULL)
		return (-1);

	if ((eap = ibuf_advance(e, sizeof(*eap))) == NULL)
		goto done;
	eap->eap_code = EAP_CODE_REQUEST;
	eap->eap_id = hdr->eap_id + 1;
	eap->eap_type = sa->sa_policy->pol_auth.auth_eap;

	switch (sa->sa_policy->pol_auth.auth_eap) {
	case EAP_TYPE_MSCHAP_V2:
		name = IKED_USER;	/* XXX should be user-configurable */
		eap->eap_length = htobe16(sizeof(*eap) +
		    sizeof(*ms) + strlen(name));

		if ((ms = ibuf_advance(e, sizeof(*ms))) == NULL)
			return (-1);
		ms->msc_opcode = EAP_MSOPCODE_CHALLENGE;
		ms->msc_id = eap->eap_id;
		ms->msc_length = htobe16(sizeof(*ms) + strlen(name));
		ms->msc_valuesize = sizeof(ms->msc_challenge);
		arc4random_buf(ms->msc_challenge, sizeof(ms->msc_challenge));
		if (ibuf_add(e, name, strlen(name)) == -1)
			goto done;

		/* Store the EAP challenge value */
		sa->sa_eap.id_type = eap->eap_type;
		if ((sa->sa_eap.id_buf = ibuf_new(ms->msc_challenge,
		    sizeof(ms->msc_challenge))) == NULL)
			goto done;
		break;
	default:
		log_debug("%s: unsupported EAP type %s", __func__,
		    print_map(eap->eap_type, eap_type_map));
		goto done;
	}

	ret = ikev2_send_ike_e(env, sa, e,
	    IKEV2_PAYLOAD_EAP, IKEV2_EXCHANGE_IKE_AUTH, 1);

 done:
	ibuf_release(e);

	return (ret);
}

int
eap_success(struct iked *env, struct iked_sa *sa, struct eap_header *hdr)
{
	struct eap_header		*resp;
	int				 ret = -1;
	struct ibuf			*e;

	if ((e = ibuf_static()) == NULL)
		return (-1);

	if ((resp = ibuf_advance(e, sizeof(*resp))) == NULL)
		goto done;
	resp->eap_code = EAP_CODE_SUCCESS;
	resp->eap_id = hdr->eap_id;
	resp->eap_length = htobe16(sizeof(*resp));

	ret = ikev2_send_ike_e(env, sa, e,
	    IKEV2_PAYLOAD_EAP, IKEV2_EXCHANGE_IKE_AUTH, 1);

 done:
	ibuf_release(e);

	return (ret);
}

int
eap_mschap(struct iked *env, struct iked_sa *sa, struct eap_message *eap)
{
	struct iked_user		*usr;
	struct eap_message		*resp;
	struct eap_mschap_response	*msr;
	struct eap_mschap_peer		*msp;
	struct eap_mschap		*ms;
	struct eap_mschap_success	*mss;
	u_int8_t			*ptr, *pass;
	size_t				 len, passlen;
	char				*name, *msg;
	u_int8_t			 ntresponse[EAP_MSCHAP_NTRESPONSE_SZ];
	u_int8_t			 successmsg[EAP_MSCHAP_SUCCESS_SZ];
	struct ibuf			*eapmsg = NULL;
	int				 ret = -1;

	if (!sa_stateok(sa, IKEV2_STATE_EAP)) {
		log_debug("%s: unexpected EAP", __func__);
		return (0);	/* ignore */
	}

	if (sa->sa_hdr.sh_initiator) {
		log_debug("%s: initiator EAP not supported", __func__);
		return (-1);
	}

	/* Only MSCHAP-V2 */
	if (eap->eap_type != EAP_TYPE_MSCHAP_V2) {
		log_debug("%s: unsupported type EAP-%s", __func__,
		    print_map(eap->eap_type, eap_type_map));
		return (-1);
	}

	if (betoh16(eap->eap_length) < (sizeof(*eap) + sizeof(*ms))) {
		log_debug("%s: short message", __func__);
		return (-1);
	}

	ms = (struct eap_mschap *)(eap + 1);
	ptr = (u_int8_t *)(eap + 1);

	switch (ms->ms_opcode) {
	case EAP_MSOPCODE_RESPONSE:
		msr = (struct eap_mschap_response *)ms;
		if (betoh16(eap->eap_length) < (sizeof(*eap) + sizeof(*msr))) {
			log_debug("%s: short response", __func__);
			return (-1);
		}
		ptr += sizeof(*msr);
		len = betoh16(eap->eap_length) -
		    sizeof(*eap) - sizeof(*msr);
		if (len == 0 && sa->sa_eapid != NULL)
			name = strdup(sa->sa_eapid);
		else
			name = get_string(ptr, len);
		if (name == NULL) {
			log_debug("%s: invalid response name", __func__);
			return (-1);
		}
		if ((usr = user_lookup(env, name)) == NULL) {
			log_debug("%s: unknown user '%s'", __func__, name);
			free(name);
			return (-1);
		}
		free(name);

		if ((pass = string2unicode(usr->usr_pass, &passlen)) == NULL)
			return (-1);

		msp = &msr->msr_response.resp_peer;
		mschap_nt_response(ibuf_data(sa->sa_eap.id_buf),
		    msp->msp_challenge, usr->usr_name, strlen(usr->usr_name),
		    pass, passlen, ntresponse);

		if (memcmp(ntresponse, msp->msp_ntresponse,
		    sizeof(ntresponse)) != 0) {
			log_debug("%s: '%s' authentication failed", __func__,
			    usr->usr_name);
			free(pass);

			/* XXX should we send an EAP failure packet? */
			return (-1);
		}

		bzero(&successmsg, sizeof(successmsg));
		mschap_auth_response(pass, passlen,
		    ntresponse, ibuf_data(sa->sa_eap.id_buf),
		    msp->msp_challenge, usr->usr_name, strlen(usr->usr_name),
		    successmsg);
		if ((sa->sa_eapmsk = ibuf_new(NULL, MSCHAP_MSK_SZ)) == NULL) {
			log_debug("%s: failed to get MSK", __func__);
			free(pass);
			return (-1);
		}
		mschap_msk(pass, passlen, ntresponse,
		    ibuf_data(sa->sa_eapmsk));
		free(pass);

		log_info("%s: '%s' authenticated", __func__, usr->usr_name);


		if ((eapmsg = ibuf_static()) == NULL)
			return (-1);

		msg = " M=Welcome";

		if ((resp = ibuf_advance(eapmsg, sizeof(*resp))) == NULL)
			goto done;
		resp->eap_code = EAP_CODE_REQUEST;
		resp->eap_id = eap->eap_id + 1;
		resp->eap_length = htobe16(sizeof(*resp) + sizeof(*mss) +
		    sizeof(successmsg) + strlen(msg));
		resp->eap_type = EAP_TYPE_MSCHAP_V2;

		if ((mss = ibuf_advance(eapmsg, sizeof(*mss))) == NULL)
			goto done;
		mss->mss_opcode = EAP_MSOPCODE_SUCCESS;
		mss->mss_id = msr->msr_id;
		mss->mss_length = htobe16(sizeof(*mss) +
		    sizeof(successmsg) + strlen(msg));
		if (ibuf_add(eapmsg, successmsg, sizeof(successmsg)) != 0)
			goto done;
		if (ibuf_add(eapmsg, msg, strlen(msg)) != 0)
			goto done;
		break;
	case EAP_MSOPCODE_SUCCESS:
		if ((eapmsg = ibuf_static()) == NULL)
			return (-1);
		if ((resp = ibuf_advance(eapmsg, sizeof(*resp))) == NULL)
			goto done;
		resp->eap_code = EAP_CODE_RESPONSE;
		resp->eap_id = eap->eap_id;
		resp->eap_length = htobe16(sizeof(*resp) + sizeof(*ms));
		resp->eap_type = EAP_TYPE_MSCHAP_V2;
		if ((ms = ibuf_advance(eapmsg, sizeof(*ms))) == NULL)
			goto done;
		ms->ms_opcode = EAP_MSOPCODE_SUCCESS;
		break;
	case EAP_MSOPCODE_FAILURE:
	case EAP_MSOPCODE_CHANGE_PASSWORD:
	case EAP_MSOPCODE_CHALLENGE:
	default:
		log_debug("%s: EAP-%s unsupported "
		    "responder operation %s", __func__,
		    print_map(eap->eap_type, eap_type_map),
		    print_map(ms->ms_opcode, eap_msopcode_map));
		return (-1);
	}

	if (eapmsg != NULL)
		ret = ikev2_send_ike_e(env, sa, eapmsg,
		    IKEV2_PAYLOAD_EAP, IKEV2_EXCHANGE_IKE_AUTH, 1);

	if (ret == 0)
		sa_state(env, sa, IKEV2_STATE_AUTH_SUCCESS);

 done:
	ibuf_release(eapmsg);
	return (ret);
}

int
eap_parse(struct iked *env, struct iked_sa *sa, void *data, int response)
{
	struct eap_header		*hdr = data;
	struct eap_message		*eap = data;
	size_t				 len;
	u_int8_t			*ptr;
	struct eap_mschap		*ms;
	struct eap_mschap_challenge	*msc;
	struct eap_mschap_response	*msr;
	struct eap_mschap_success	*mss;
	struct eap_mschap_failure	*msf;
	char				*str;

	/* length is already verified by the caller */
	len = betoh16(hdr->eap_length);
	ptr = (u_int8_t *)(eap + 1);

	switch (hdr->eap_code) {
	case EAP_CODE_REQUEST:
	case EAP_CODE_RESPONSE:
		if (len < sizeof(*eap)) {
			log_debug("%s: short message", __func__);
			return (-1);
		}
		break;
	case EAP_CODE_SUCCESS:
		return (0);
	case EAP_CODE_FAILURE:
		if (response)
			return (0);
		return (-1);
	default:
		log_debug("%s: unsupported EAP code %s", __func__,
		    print_map(hdr->eap_code, eap_code_map));
		return (-1);
	}

	switch (eap->eap_type) {
	case EAP_TYPE_IDENTITY:
		if (eap->eap_code == EAP_CODE_REQUEST)
			break;
		if ((str = eap_identity_response(eap)) == NULL)
			return (-1);
		if (response) {
			free(str);
			break;
		}
		if (sa->sa_eapid != NULL) {
			free(str);
			log_debug("%s: EAP identity already known", __func__);
			return (0);
		}
		sa->sa_eapid = str;
		return (eap_challenge_request(env, sa, hdr));
	case EAP_TYPE_MSCHAP_V2:
		ms = (struct eap_mschap *)ptr;
		switch (ms->ms_opcode) {
		case EAP_MSOPCODE_CHALLENGE:
			msc = (struct eap_mschap_challenge *)ptr;
			ptr += sizeof(*msc);
			len = betoh16(eap->eap_length) -
			    sizeof(*eap) - sizeof(*msc);
			if ((str = get_string(ptr, len)) == NULL) {
				log_debug("%s: invalid challenge name",
				    __func__);
				return (-1);
			}
			log_info("%s: %s %s id %d "
			    "length %d valuesize %d name '%s' length %zu",
			    __func__,
			    print_map(eap->eap_type, eap_type_map),
			    print_map(ms->ms_opcode, eap_msopcode_map),
			    msc->msc_id, betoh16(msc->msc_length),
			    msc->msc_valuesize, str, len);
			free(str);
			print_hex(msc->msc_challenge, 0,
			    sizeof(msc->msc_challenge));
			break;
		case EAP_MSOPCODE_RESPONSE:
			msr = (struct eap_mschap_response *)ptr;
			ptr += sizeof(*msr);
			len = betoh16(eap->eap_length) -
			    sizeof(*eap) - sizeof(*msr);
			if ((str = get_string(ptr, len)) == NULL) {
				log_debug("%s: invalid response name",
				    __func__);
				return (-1);
			}
			log_info("%s: %s %s id %d "
			    "length %d valuesize %d name '%s' name-length %zu",
			    __func__,
			    print_map(eap->eap_type, eap_type_map),
			    print_map(ms->ms_opcode, eap_msopcode_map),
			    msr->msr_id, betoh16(msr->msr_length),
			    msr->msr_valuesize, str, len);
			free(str);
			print_hex(msr->msr_response.resp_data, 0,
			    sizeof(msr->msr_response.resp_data));
			break;
		case EAP_MSOPCODE_SUCCESS:
			if (eap->eap_code == EAP_CODE_REQUEST) {
				mss = (struct eap_mschap_success *)ptr;
				ptr += sizeof(*mss);
				len = betoh16(eap->eap_length) -
				    sizeof(*eap) - sizeof(*mss);
				if ((str = get_string(ptr, len)) == NULL) {
					log_debug("%s: invalid response name",
					    __func__);
					return (-1);
				}
				log_info("%s: %s %s request id %d "
				    "length %d message '%s' message-len %zu",
				    __func__,
				    print_map(eap->eap_type, eap_type_map),
				    print_map(ms->ms_opcode, eap_msopcode_map),
				    mss->mss_id, betoh16(mss->mss_length),
				    str, len);
				free(str);
			} else {
				ms = (struct eap_mschap *)ptr;
				log_info("%s: %s %s response", __func__,
				    print_map(eap->eap_type, eap_type_map),
				    print_map(ms->ms_opcode, eap_msopcode_map));
				if (response)
					break;
				if (!sa_stateok(sa, IKEV2_STATE_AUTH_SUCCESS))
					return (-1);

				return (eap_success(env, sa, hdr));
			}
			break;
		case EAP_MSOPCODE_FAILURE:
			msf = (struct eap_mschap_failure *)ptr;
			ptr += sizeof(*msf);
			len = betoh16(eap->eap_length) -
			    sizeof(*eap) - sizeof(*msf);
			if ((str = get_string(ptr, len)) == NULL) {
				log_debug("%s: invalid failure message",
				    __func__);
				return (-1);
			}
			log_info("%s: %s %s id %d "
			    "length %d message '%s'", __func__,
			    print_map(eap->eap_type, eap_type_map),
			    print_map(ms->ms_opcode, eap_msopcode_map),
			    msf->msf_id, betoh16(msf->msf_length), str);
			free(str);
			break;
		default:
			log_info("%s: unknown ms opcode %d", __func__,
			    ms->ms_opcode);
			return (-1);
		}
		if (response)
			break;

		return (eap_mschap(env, sa, eap));
	default:
		log_debug("%s: unsupported EAP type %s", __func__,
		    print_map(eap->eap_type, eap_type_map));
		return (-1);
	}

	return (0);
}

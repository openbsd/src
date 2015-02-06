/*	$OpenBSD: ikev2.c,v 1.119 2015/02/06 10:39:01 deraadt Exp $	*/

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

#include <sys/param.h>	/* roundup */
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
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>

#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "iked.h"
#include "ikev2.h"
#include "eap.h"
#include "dh.h"

int	 ikev2_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 ikev2_dispatch_ikev1(int, struct privsep_proc *, struct imsg *);
int	 ikev2_dispatch_cert(int, struct privsep_proc *, struct imsg *);

struct iked_sa *
	 ikev2_getimsgdata(struct iked *, struct imsg *, struct iked_sahdr *,
	    u_int8_t *, u_int8_t **, size_t *);

void	 ikev2_recv(struct iked *, struct iked_message *);
int	 ikev2_ike_auth_recv(struct iked *, struct iked_sa *,
	    struct iked_message *);
int	 ikev2_ike_auth(struct iked *, struct iked_sa *);

void	 ikev2_init_recv(struct iked *, struct iked_message *,
	    struct ike_header *);
int	 ikev2_init_ike_sa_peer(struct iked *, struct iked_policy *,
	    struct iked_addr *);
int	 ikev2_init_ike_auth(struct iked *, struct iked_sa *);
int	 ikev2_init_auth(struct iked *, struct iked_message *);
int	 ikev2_init_done(struct iked *, struct iked_sa *);

void	 ikev2_resp_recv(struct iked *, struct iked_message *,
	    struct ike_header *);
int	 ikev2_resp_ike_sa_init(struct iked *, struct iked_message *);
int	 ikev2_resp_ike_auth(struct iked *, struct iked_sa *);
int	 ikev2_resp_ike_eap(struct iked *, struct iked_sa *, struct ibuf *);

int	 ikev2_send_create_child_sa(struct iked *, struct iked_sa *,
	    struct iked_spi *, u_int8_t);
int	 ikev2_ikesa_enable(struct iked *, struct iked_sa *, struct iked_sa *);
void	 ikev2_ikesa_delete(struct iked *, struct iked_sa *, int);
int	 ikev2_init_create_child_sa(struct iked *, struct iked_message *);
int	 ikev2_resp_create_child_sa(struct iked *, struct iked_message *);
void	 ikev2_ike_sa_rekey(struct iked *, void *);
void	 ikev2_ike_sa_timeout(struct iked *env, void *);
void	 ikev2_ike_sa_alive(struct iked *, void *);

int	 ikev2_sa_initiator(struct iked *, struct iked_sa *,
	    struct iked_sa *, struct iked_message *);
int	 ikev2_sa_responder(struct iked *, struct iked_sa *, struct iked_sa *,
	    struct iked_message *);
int	 ikev2_sa_initiator_dh(struct iked_sa *, struct iked_message *, u_int);
int	 ikev2_sa_responder_dh(struct iked_kex *, struct iked_proposals *,
	    struct iked_message *, u_int);
void	 ikev2_sa_cleanup_dh(struct iked_sa *);
int	 ikev2_sa_keys(struct iked *, struct iked_sa *, struct ibuf *);
int	 ikev2_sa_tag(struct iked_sa *, struct iked_id *);
int	 ikev2_set_sa_proposal(struct iked_sa *, struct iked_policy *, u_int);

int	 ikev2_childsa_negotiate(struct iked *, struct iked_sa *,
	    struct iked_kex *, struct iked_proposals *, int, int);
int	 ikev2_match_proposals(struct iked_proposal *, struct iked_proposal *,
	    struct iked_transform **);
int	 ikev2_valid_proposal(struct iked_proposal *,
	    struct iked_transform **, struct iked_transform **, int *);

ssize_t	 ikev2_add_proposals(struct iked *, struct iked_sa *, struct ibuf *,
	    struct iked_proposals *, u_int8_t, int, int);
ssize_t	 ikev2_add_cp(struct iked *, struct iked_sa *, struct ibuf *);
ssize_t	 ikev2_add_transform(struct ibuf *,
	    u_int8_t, u_int8_t, u_int16_t, u_int16_t);
ssize_t	 ikev2_add_ts(struct ibuf *, struct ikev2_payload **, ssize_t,
	    struct iked_sa *, int);
ssize_t	 ikev2_add_certreq(struct ibuf *, struct ikev2_payload **, ssize_t,
	    struct ibuf *, u_int8_t);
ssize_t	 ikev2_add_ipcompnotify(struct iked *, struct ibuf *,
	    struct ikev2_payload **, ssize_t, struct iked_sa *);
ssize_t	 ikev2_add_ts_payload(struct ibuf *, u_int, struct iked_sa *);
int	 ikev2_add_data(struct ibuf *, void *, size_t);
int	 ikev2_add_buf(struct ibuf *buf, struct ibuf *);

int	 ikev2_ipcomp_enable(struct iked *, struct iked_sa *);
void	 ikev2_ipcomp_csa_free(struct iked *, struct iked_childsa *);

int	 ikev2_cp_setaddr(struct iked *, struct iked_sa *);
int	 ikev2_cp_fixaddr(struct iked_sa *, struct iked_addr *,
	    struct iked_addr *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ikev2_dispatch_parent },
	{ "ikev1",	PROC_IKEV1,	ikev2_dispatch_ikev1 },
	{ "certstore",	PROC_CERT,	ikev2_dispatch_cert }
};

pid_t
ikev2(struct privsep *ps, struct privsep_proc *p)
{
	return (proc_run(ps, p, procs, nitems(procs), NULL, NULL));
}

int
ikev2_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;

	switch (imsg->hdr.type) {
	case IMSG_CTL_RESET:
		return (config_getreset(env, imsg));
	case IMSG_CTL_COUPLE:
	case IMSG_CTL_DECOUPLE:
		return (config_getcoupled(env, imsg->hdr.type));
	case IMSG_CTL_ACTIVE:
	case IMSG_CTL_PASSIVE:
		if (config_getmode(env, imsg->hdr.type) == -1)
			return (0);	/* ignore error */
		timer_set(env, &env->sc_inittmr, ikev2_init_ike_sa,
		    NULL);
		timer_add(env, &env->sc_inittmr, IKED_INITIATOR_INITIAL);
		return (0);
	case IMSG_UDP_SOCKET:
		return (config_getsocket(env, imsg, ikev2_msg_cb));
	case IMSG_PFKEY_SOCKET:
		return (config_getpfkey(env, imsg));
	case IMSG_CFG_POLICY:
		return (config_getpolicy(env, imsg));
	case IMSG_CFG_USER:
		return (config_getuser(env, imsg));
	case IMSG_COMPILE:
		return (config_getcompile(env, imsg));
	default:
		break;
	}

	return (-1);
}

int
ikev2_dispatch_ikev1(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;
	struct iked_message	 msg;
	u_int8_t		*buf;
	ssize_t			 len;

	switch (imsg->hdr.type) {
	case IMSG_IKE_MESSAGE:
		log_debug("%s: message", __func__);
		IMSG_SIZE_CHECK(imsg, &msg);
		memcpy(&msg, imsg->data, sizeof(msg));

		len = IMSG_DATA_SIZE(imsg) - sizeof(msg);
		buf = (u_int8_t *)imsg->data + sizeof(msg);
		if (len <= 0 || (msg.msg_data = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: short message", __func__);
			return (0);
		}

		log_debug("%s: message length %zd", __func__, len);

		ikev2_recv(env, &msg);
		ikev2_msg_cleanup(env, &msg);
		return (0);
	default:
		break;
	}

	return (-1);
}

int
ikev2_dispatch_cert(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;
	struct iked_sahdr	 sh;
	struct iked_sa		*sa;
	u_int8_t		 type;
	u_int8_t		*ptr;
	size_t			 len;
	struct iked_id		*id = NULL;
	int			 ignore = 0;

	switch (imsg->hdr.type) {
	case IMSG_CERTREQ:
		IMSG_SIZE_CHECK(imsg, &type);

		ptr = imsg->data;
		memcpy(&type, ptr, sizeof(type));
		ptr += sizeof(type);

		ibuf_release(env->sc_certreq);
		env->sc_certreqtype = type;
		env->sc_certreq = ibuf_new(ptr,
		    IMSG_DATA_SIZE(imsg) - sizeof(type));

		log_debug("%s: updated local CERTREQ type %s length %zu",
		    __func__, print_map(type, ikev2_cert_map),
		    ibuf_length(env->sc_certreq));

		break;
	case IMSG_CERTVALID:
	case IMSG_CERTINVALID:
		memcpy(&sh, imsg->data, sizeof(sh));
		memcpy(&type, (u_int8_t *)imsg->data + sizeof(sh),
		    sizeof(type));

		/* Ignore invalid or unauthenticated SAs */
		if ((sa = sa_lookup(env,
		    sh.sh_ispi, sh.sh_rspi, sh.sh_initiator)) == NULL ||
		    sa->sa_state < IKEV2_STATE_EAP)
			break;

		if (imsg->hdr.type == IMSG_CERTVALID) {
			log_debug("%s: peer certificate is valid", __func__);
			sa_stateflags(sa, IKED_REQ_CERTVALID);
		} else {
			log_warnx("%s: peer certificate is invalid", __func__);
		}

		if (ikev2_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send ike auth", __func__);
		break;
	case IMSG_CERT:
		if ((sa = ikev2_getimsgdata(env, imsg,
		    &sh, &type, &ptr, &len)) == NULL) {
			log_debug("%s: invalid cert reply", __func__);
			break;
		}

		/*
		 * Ignore the message if we already got a valid certificate.
		 * This might happen if the peer sent multiple CERTREQs.
		 */
		if (sa->sa_stateflags & IKED_REQ_CERT ||
		    type == IKEV2_CERT_NONE)
			ignore = 1;

		log_debug("%s: cert type %s length %zu, %s", __func__,
		    print_map(type, ikev2_cert_map), len,
		    ignore ? "ignored" : "ok");

		if (ignore)
			break;

		if (sh.sh_initiator)
			id = &sa->sa_icert;
		else
			id = &sa->sa_rcert;

		id->id_type = type;
		id->id_offset = 0;
		ibuf_release(id->id_buf);
		id->id_buf = NULL;

		if (len <= 0 || (id->id_buf = ibuf_new(ptr, len)) == NULL) {
			log_debug("%s: failed to get cert payload",
			    __func__);
			break;
		}

		sa_stateflags(sa, IKED_REQ_CERT);

		if (ikev2_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send ike auth", __func__);
		break;
	case IMSG_AUTH:
		if ((sa = ikev2_getimsgdata(env, imsg,
		    &sh, &type, &ptr, &len)) == NULL) {
			log_debug("%s: invalid auth reply", __func__);
			break;
		}
		if (sa_stateok(sa, IKEV2_STATE_VALID)) {
			log_warnx("%s: ignoring AUTH in state %s", __func__,
			    print_map(sa->sa_state, ikev2_state_map));
			break;
		}

		log_debug("%s: AUTH type %d len %zu", __func__, type, len);

		id = &sa->sa_localauth;
		id->id_type = type;
		id->id_offset = 0;
		ibuf_release(id->id_buf);

		if (type != IKEV2_AUTH_NONE) {
			if (len <= 0 ||
			    (id->id_buf = ibuf_new(ptr, len)) == NULL) {
				log_debug("%s: failed to get auth payload",
				    __func__);
				break;
			}
		}

		sa_stateflags(sa, IKED_REQ_AUTH);

		if (ikev2_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send ike auth", __func__);
		break;
	default:
		return (-1);
	}

	return (0);
}

struct iked_sa *
ikev2_getimsgdata(struct iked *env, struct imsg *imsg, struct iked_sahdr *sh,
    u_int8_t *type, u_int8_t **buf, size_t *size)
{
	u_int8_t	*ptr;
	size_t		 len;
	struct iked_sa	*sa;

	IMSG_SIZE_CHECK(imsg, sh);

	ptr = imsg->data;
	len = IMSG_DATA_SIZE(imsg) - sizeof(*sh) - sizeof(*type);
	memcpy(sh, ptr, sizeof(*sh));
	memcpy(type, ptr + sizeof(*sh), sizeof(*type));

	sa = sa_lookup(env, sh->sh_ispi, sh->sh_rspi, sh->sh_initiator);

	log_debug("%s: imsg %d rspi %s ispi %s initiator %d sa %s"
	    " type %d data length %zd",
	    __func__, imsg->hdr.type,
	    print_spi(sh->sh_rspi, 8),
	    print_spi(sh->sh_ispi, 8),
	    sh->sh_initiator,
	    sa == NULL ? "invalid" : "valid", *type, len);

	if (sa == NULL)
		return (NULL);

	*buf = ptr + sizeof(*sh) + sizeof(*type);
	*size = len;

	return (sa);
}

void
ikev2_recv(struct iked *env, struct iked_message *msg)
{
	struct ike_header	*hdr;
	struct iked_message	*m;
	struct iked_sa		*sa;
	u_int			 initiator, flag = 0;

	hdr = ibuf_seek(msg->msg_data, msg->msg_offset, sizeof(*hdr));

	if (hdr == NULL || ibuf_size(msg->msg_data) <
	    (betoh32(hdr->ike_length) - msg->msg_offset))
		return;

	initiator = (hdr->ike_flags & IKEV2_FLAG_INITIATOR) ? 0 : 1;
	msg->msg_response = (hdr->ike_flags & IKEV2_FLAG_RESPONSE) ? 1 : 0;
	msg->msg_sa = sa_lookup(env,
	    betoh64(hdr->ike_ispi), betoh64(hdr->ike_rspi),
	    initiator);
	msg->msg_msgid = betoh32(hdr->ike_msgid);
	if (policy_lookup(env, msg) != 0)
		return;

	log_info("%s: %s %s from %s %s to %s policy '%s' id %u, %ld bytes",
	    __func__, print_map(hdr->ike_exchange, ikev2_exchange_map),
	    msg->msg_response ? "response" : "request",
	    initiator ? "responder" : "initiator",
	    print_host((struct sockaddr *)&msg->msg_peer, NULL, 0),
	    print_host((struct sockaddr *)&msg->msg_local, NULL, 0),
	    msg->msg_policy->pol_name, msg->msg_msgid,
	    ibuf_length(msg->msg_data));
	log_debug("%s: ispi %s rspi %s", __func__,
	    print_spi(betoh64(hdr->ike_ispi), 8),
	    print_spi(betoh64(hdr->ike_rspi), 8));

	if ((sa = msg->msg_sa) == NULL)
		goto done;

	if (hdr->ike_exchange == IKEV2_EXCHANGE_CREATE_CHILD_SA)
		flag = IKED_REQ_CHILDSA;
	if (hdr->ike_exchange == IKEV2_EXCHANGE_INFORMATIONAL)
		flag = IKED_REQ_INF;

	if (msg->msg_response) {
		if (msg->msg_msgid > sa->sa_reqid)
			return;
		if (hdr->ike_exchange != IKEV2_EXCHANGE_INFORMATIONAL &&
		    !ikev2_msg_lookup(env, &sa->sa_requests, msg, hdr))
			return;
		if (flag) {
			if ((sa->sa_stateflags & flag) == 0)
				return;
			/*
			 * We have initiated this exchange, even if
			 * we are not the initiator of the IKE SA.
			 */
			initiator = 1;
		}
		/*
		 * There's no need to keep the request around anymore
		 */
		if ((m = ikev2_msg_lookup(env, &sa->sa_requests, msg, hdr)))
			ikev2_msg_dispose(env, &sa->sa_requests, m);
	} else {
		if (msg->msg_msgid < sa->sa_msgid)
			return;
		if (flag)
			initiator = 0;
		/*
		 * See if we have responded to this request before
		 */
		if ((m = ikev2_msg_lookup(env, &sa->sa_responses, msg, hdr))) {
			if (ikev2_msg_retransmit_response(env, sa, m)) {
				log_warn("%s: failed to retransmit a "
				    "response", __func__);
				sa_free(env, sa);
			}
			return;
		} else if (sa->sa_msgid_set && msg->msg_msgid == sa->sa_msgid) {
			/*
			 * Response is being worked on, most likely we're
			 * waiting for the CA process to get back to us
			 */
			return;
		}
		/*
		 * If it's a new request, make sure to update the peer's
		 * message ID and dispose of all previous responses.
		 * We need to set sa_msgid_set in order to distinguish between
		 * "last msgid was 0" and "msgid not set yet".
		 */
		sa->sa_msgid = msg->msg_msgid;
		sa->sa_msgid_set = 1;
		ikev2_msg_prevail(env, &sa->sa_responses, msg);
	}

	if (sa_address(sa, &sa->sa_peer, &msg->msg_peer,
	    sa->sa_hdr.sh_initiator) == -1 ||
	    sa_address(sa, &sa->sa_local, &msg->msg_local,
	    sa->sa_hdr.sh_initiator) == -1)
		return;

	sa->sa_fd = msg->msg_fd;

	log_debug("%s: updated SA to peer %s local %s", __func__,
	    print_host((struct sockaddr *)&sa->sa_peer.addr, NULL, 0),
	    print_host((struct sockaddr *)&sa->sa_local.addr, NULL, 0));

done:
	if (initiator)
		ikev2_init_recv(env, msg, hdr);
	else
		ikev2_resp_recv(env, msg, hdr);

	if (sa != NULL && sa->sa_state == IKEV2_STATE_CLOSED) {
		log_debug("%s: closing SA", __func__);
		sa_free(env, sa);
	}
}

int
ikev2_ike_auth_recv(struct iked *env, struct iked_sa *sa,
    struct iked_message *msg)
{
	struct iked_id		*id, *certid;
	struct ibuf		*authmsg;
	struct iked_auth	 ikeauth;
	struct iked_policy	*policy = sa->sa_policy;
	int			 ret = -1;

	if (sa->sa_hdr.sh_initiator) {
		id = &sa->sa_rid;
		certid = &sa->sa_rcert;
	} else {
		id = &sa->sa_iid;
		certid = &sa->sa_icert;
	}
	/* try to relookup the policy based on the peerid */
	if (msg->msg_id.id_type && !sa->sa_hdr.sh_initiator) {
		struct iked_policy	*old = sa->sa_policy;

		sa->sa_policy = NULL;
		if (policy_lookup(env, msg) == 0 && msg->msg_policy &&
		    msg->msg_policy != old) {
			log_debug("%s: policy switch %p/%s to %p/%s",
			    __func__, old, old->pol_name,
			    msg->msg_policy, msg->msg_policy->pol_name);
			RB_REMOVE(iked_sapeers, &old->pol_sapeers, sa);
			if (RB_INSERT(iked_sapeers,
			    &msg->msg_policy->pol_sapeers, sa)) {
				/* failed, restore */
				log_debug("%s: conflicting sa", __func__);
				RB_INSERT(iked_sapeers, &old->pol_sapeers, sa);
				msg->msg_policy = old;
			} else
				policy_unref(env, old);
			policy = sa->sa_policy = msg->msg_policy;
		} else {
			/* restore */
			msg->msg_policy = sa->sa_policy = old;
		}
	}

	if (msg->msg_id.id_type) {
		memcpy(id, &msg->msg_id, sizeof(*id));
		bzero(&msg->msg_id, sizeof(msg->msg_id));

		if (!sa->sa_hdr.sh_initiator) {
			if ((authmsg = ikev2_msg_auth(env, sa,
			    !sa->sa_hdr.sh_initiator)) == NULL) {
				log_debug("%s: failed to get response "
				    "auth data", __func__);
				return (-1);
			}

			ca_setauth(env, sa, authmsg, PROC_CERT);
			ibuf_release(authmsg);
		}
	}

	if (msg->msg_cert.id_type) {
		memcpy(certid, &msg->msg_cert, sizeof(*certid));
		bzero(&msg->msg_cert, sizeof(msg->msg_cert));

		ca_setcert(env, &sa->sa_hdr,
		    id, certid->id_type,
		    ibuf_data(certid->id_buf),
		    ibuf_length(certid->id_buf), PROC_CERT);
	}

	if (msg->msg_auth.id_type) {
		memcpy(&ikeauth, &policy->pol_auth, sizeof(ikeauth));

		if (policy->pol_auth.auth_eap && sa->sa_eapmsk != NULL) {
			/*
			 * The initiator EAP auth is a PSK derived
			 * from the EAP-specific MSK
			 */
			ikeauth.auth_method = IKEV2_AUTH_SHARED_KEY_MIC;

			/* Copy session key as PSK */
			memcpy(ikeauth.auth_data, ibuf_data(sa->sa_eapmsk),
			    ibuf_size(sa->sa_eapmsk));
			ikeauth.auth_length = ibuf_size(sa->sa_eapmsk);
		}

		if (msg->msg_auth.id_type != ikeauth.auth_method) {
			log_warnx("%s: unexpected auth method %s", __func__,
			    print_map(ikeauth.auth_method, ikev2_auth_map));
			return (-1);
		}

		if ((authmsg = ikev2_msg_auth(env, sa,
		    sa->sa_hdr.sh_initiator)) == NULL) {
			log_debug("%s: failed to get auth data", __func__);
			return (-1);
		}

		ret = ikev2_msg_authverify(env, sa, &ikeauth,
		    ibuf_data(msg->msg_auth.id_buf),
		    ibuf_length(msg->msg_auth.id_buf),
		    authmsg);
		ibuf_release(authmsg);

		if (ret != 0) {
			log_debug("%s: ikev2_msg_authverify failed", __func__);
			return (-1);
		}

		if (sa->sa_eapmsk != NULL) {
			if ((authmsg = ikev2_msg_auth(env, sa,
			    !sa->sa_hdr.sh_initiator)) == NULL) {
				log_debug("%s: failed to get auth data",
				    __func__);
				return (-1);
			}

			/* XXX 2nd AUTH for EAP messages */
			ret = ikev2_msg_authsign(env, sa, &ikeauth, authmsg);
			ibuf_release(authmsg);

			if (ret != 0) {
				/* XXX */
				return (-1);
			}

			/* ikev2_msg_authverify verified AUTH */
			sa_stateflags(sa, IKED_REQ_AUTHVALID);
			sa_stateflags(sa, IKED_REQ_EAPVALID);

			sa_state(env, sa, IKEV2_STATE_EAP_SUCCESS);
		}
	}

	if (!TAILQ_EMPTY(&msg->msg_proposals)) {
		if (ikev2_sa_negotiate(&sa->sa_proposals,
		    &sa->sa_policy->pol_proposals, &msg->msg_proposals) != 0) {
			log_debug("%s: no proposal chosen", __func__);
			msg->msg_error = IKEV2_N_NO_PROPOSAL_CHOSEN;
			return (-1);
		} else
			sa_stateflags(sa, IKED_REQ_SA);
	}

	return ikev2_ike_auth(env, sa);
}

int
ikev2_ike_auth(struct iked *env, struct iked_sa *sa)
{
	/* Attempt state transition */
	if (sa->sa_state == IKEV2_STATE_EAP_SUCCESS)
		sa_state(env, sa, IKEV2_STATE_EAP_VALID);
	else if (sa->sa_state == IKEV2_STATE_AUTH_SUCCESS)
		sa_state(env, sa, IKEV2_STATE_VALID);

	if (sa->sa_hdr.sh_initiator) {
		if (sa_stateok(sa, IKEV2_STATE_AUTH_SUCCESS))
			return (ikev2_init_done(env, sa));
		else
			return (ikev2_init_ike_auth(env, sa));
	}
	return (ikev2_resp_ike_auth(env, sa));
}

void
ikev2_init_recv(struct iked *env, struct iked_message *msg,
    struct ike_header *hdr)
{
	struct iked_sa		*sa;
	in_port_t		 port;
	struct iked_socket	*sock;

	if (ikev2_msg_valid_ike_sa(env, hdr, msg) == -1) {
		log_debug("%s: unknown SA", __func__);
		return;
	}
	sa = msg->msg_sa;

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		/* Update the SPIs */
		if ((sa = sa_new(env,
		    betoh64(hdr->ike_ispi), betoh64(hdr->ike_rspi), 1,
		    NULL)) == NULL || sa != msg->msg_sa) {
			log_debug("%s: invalid new SA", __func__);
			if (sa)
				sa_free(env, sa);
		}
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		if (ikev2_msg_valid_ike_sa(env, hdr, msg) == -1)
			return;
		break;
	case IKEV2_EXCHANGE_INFORMATIONAL:
		break;
	default:
		log_debug("%s: unsupported exchange: %s", __func__,
		    print_map(hdr->ike_exchange, ikev2_exchange_map));
		return;
	}

	if (ikev2_pld_parse(env, hdr, msg, msg->msg_offset) != 0) {
		log_debug("%s: failed to parse message", __func__);
		return;
	}

	if (!ikev2_msg_frompeer(msg))
		return;

	if (sa->sa_udpencap && sa->sa_natt == 0 &&
	    (sock = ikev2_msg_getsocket(env,
	    sa->sa_local.addr_af, 1)) != NULL) {
		/*
		 * Update address information and use the NAT-T
		 * port and socket, if available.
		 */
		port = htons(socket_getport(
		    (struct sockaddr *)&sock->sock_addr));
		sa->sa_local.addr_port = port;
		sa->sa_peer.addr_port = port;
		(void)socket_af((struct sockaddr *)&sa->sa_local.addr, port);
		(void)socket_af((struct sockaddr *)&sa->sa_peer.addr, port);

		msg->msg_fd = sa->sa_fd = sock->sock_fd;
		msg->msg_sock = sock;
		sa->sa_natt = 1;

		log_debug("%s: NAT detected, updated SA to "
		    "peer %s local %s", __func__,
		    print_host((struct sockaddr *)&sa->sa_peer.addr, NULL, 0),
		    print_host((struct sockaddr *)&sa->sa_local.addr, NULL, 0));
	}

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		(void)ikev2_init_auth(env, msg);
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
		(void)ikev2_ike_auth_recv(env, sa, msg);
		break;
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		(void)ikev2_init_create_child_sa(env, msg);
		break;
	case IKEV2_EXCHANGE_INFORMATIONAL:
		sa->sa_stateflags &= ~IKED_REQ_INF;
		break;
	default:
		log_debug("%s: exchange %s not implemented", __func__,
		    print_map(hdr->ike_exchange, ikev2_exchange_map));
		break;
	}
}

void
ikev2_init_ike_sa(struct iked *env, void *arg)
{
	struct iked_policy	*pol;

	TAILQ_FOREACH(pol, &env->sc_policies, pol_entry) {
		if ((pol->pol_flags & IKED_POLICY_ACTIVE) == 0)
			continue;
		if (sa_peer_lookup(pol, &pol->pol_peer.addr) != NULL) {
			log_debug("%s: \"%s\" is already active",
			    __func__, pol->pol_name);
			continue;
		}

		log_debug("%s: initiating \"%s\"", __func__, pol->pol_name);

		if (ikev2_init_ike_sa_peer(env, pol, &pol->pol_peer))
			log_debug("%s: failed to initiate with peer %s",
			    __func__,
			    print_host((struct sockaddr *)&pol->pol_peer.addr,
			    NULL, 0));
	}

	timer_set(env, &env->sc_inittmr, ikev2_init_ike_sa, NULL);
	timer_add(env, &env->sc_inittmr, IKED_INITIATOR_INTERVAL);
}

int
ikev2_init_ike_sa_peer(struct iked *env, struct iked_policy *pol,
    struct iked_addr *peer)
{
	struct sockaddr_storage		 ss;
	struct iked_message		 req;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ikev2_keyexchange	*ke;
	struct ikev2_notify		*n;
	struct iked_sa			*sa;
	struct ibuf			*buf;
	struct group			*group;
	u_int8_t			*ptr;
	ssize_t				 len;
	int				 ret = -1;
	struct iked_socket		*sock;
	in_port_t			 port;

	if ((sock = ikev2_msg_getsocket(env, peer->addr_af, 0)) == NULL)
		return (-1);

	/* Create a new initiator SA */
	if ((sa = sa_new(env, 0, 0, 1, pol)) == NULL)
		return (-1);

	/* Pick peer's DH group if asked */
	/* XXX free old sa_dhgroup ? */
	sa->sa_dhgroup = pol->pol_peerdh;

	if (ikev2_sa_initiator(env, sa, NULL, NULL) == -1)
		goto done;

	if (pol->pol_local.addr.ss_family == AF_UNSPEC) {
		if (socket_getaddr(sock->sock_fd, &ss) == -1)
			goto done;
	} else
		memcpy(&ss, &pol->pol_local.addr, pol->pol_local.addr.ss_len);

	if ((buf = ikev2_msg_init(env, &req, &peer->addr, peer->addr.ss_len,
	    &ss, ss.ss_len, 0)) == NULL)
		goto done;

	/* Inherit the port from the 1st send socket */
	port = htons(socket_getport((struct sockaddr *)&sock->sock_addr));
	(void)socket_af((struct sockaddr *)&req.msg_local, port);
	(void)socket_af((struct sockaddr *)&req.msg_peer, port);

	req.msg_fd = sock->sock_fd;
	req.msg_sa = sa;
	req.msg_sock = sock;
	req.msg_msgid = ikev2_msg_id(env, sa);

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, sa, req.msg_msgid,
	    IKEV2_PAYLOAD_SA, IKEV2_EXCHANGE_IKE_SA_INIT, 0)) == NULL)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(env, sa, buf, &pol->pol_proposals,
	    IKEV2_SAPROTO_IKE, sa->sa_hdr.sh_initiator, 0)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_KE) == -1)
		goto done;

	/* KE payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((ke = ibuf_advance(buf, sizeof(*ke))) == NULL)
		goto done;
	if ((group = sa->sa_dhgroup) == NULL) {
		log_debug("%s: invalid dh", __func__);
		goto done;
	}
	ke->kex_dhgroup = htobe16(group->id);
	if (ikev2_add_buf(buf, sa->sa_dhiexchange) == -1)
		goto done;
	len = sizeof(*ke) + dh_getlen(group);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if (ikev2_add_buf(buf, sa->sa_inonce) == -1)
		goto done;
	len = ibuf_size(sa->sa_inonce);

	if ((env->sc_opts & IKED_OPT_NONATT) == 0) {
		if (ntohs(port) == IKED_NATT_PORT) {
			/* Enforce NAT-T on the initiator side */
			log_debug("%s: enforcing NAT-T", __func__);
			req.msg_natt = sa->sa_natt = 1;
		}

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		/* NAT-T notify payloads */
		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_NAT_DETECTION_SOURCE_IP);
		len = ikev2_nat_detection(env, &req, NULL, 0, 0);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(env, &req, ptr, len,
		    betoh16(n->n_type))) == -1)
			goto done;
		len += sizeof(*n);

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_NAT_DETECTION_DESTINATION_IP);
		len = ikev2_nat_detection(env, &req, NULL, 0, 0);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(env, &req, ptr, len,
		    betoh16(n->n_type))) == -1)
			goto done;
		len += sizeof(*n);
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	(void)ikev2_pld_parse(env, hdr, &req, 0);

	ibuf_release(sa->sa_1stmsg);
	if ((sa->sa_1stmsg = ibuf_dup(buf)) == NULL) {
		log_debug("%s: failed to copy 1st message", __func__);
		goto done;
	}

	memcpy(&sa->sa_polpeer, &pol->pol_peer, sizeof(sa->sa_polpeer));
	if (RB_INSERT(iked_sapeers, &pol->pol_sapeers, sa)) {
		log_debug("%s: conflicting sa", __func__);
		goto done;
	}

	if ((ret = ikev2_msg_send(env, &req)) == 0)
		sa_state(env, sa, IKEV2_STATE_SA_INIT);

 done:
	if (ret == -1) {
		log_debug("%s: closing SA", __func__);
		sa_free(env, sa);
	}
	ikev2_msg_cleanup(env, &req);

	return (ret);
}

int
ikev2_init_auth(struct iked *env, struct iked_message *msg)
{
	struct iked_sa			*sa = msg->msg_sa;
	struct ibuf			*authmsg;

	if (sa == NULL)
		return (-1);

	if (ikev2_sa_initiator(env, sa, NULL, msg) == -1) {
		log_debug("%s: failed to get IKE keys", __func__);
		return (-1);
	}

	if ((authmsg = ikev2_msg_auth(env, sa,
	    !sa->sa_hdr.sh_initiator)) == NULL) {
		log_debug("%s: failed to get auth data", __func__);
		return (-1);
	}

	if (ca_setauth(env, sa, authmsg, PROC_CERT) == -1) {
		log_debug("%s: failed to get cert", __func__);
		return (-1);
	}

	return (ikev2_init_ike_auth(env, sa));
}

int
ikev2_init_ike_auth(struct iked *env, struct iked_sa *sa)
{
	struct iked_policy		*pol = sa->sa_policy;
	struct ikev2_payload		*pld;
	struct ikev2_cert		*cert;
	struct ikev2_auth		*auth;
	struct iked_id			*id, *certid;
	struct ibuf			*e = NULL;
	u_int8_t			 firstpayload;
	int				 ret = -1;
	ssize_t				 len;

	if (!sa_stateok(sa, IKEV2_STATE_SA_INIT))
		return (0);

	if (!sa->sa_localauth.id_type) {
		log_debug("%s: no local auth", __func__);
		return (-1);
	}

	/* New encrypted message buffer */
	if ((e = ibuf_static()) == NULL)
		goto done;

	id = &sa->sa_iid;
	certid = &sa->sa_icert;

	/* ID payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	firstpayload = IKEV2_PAYLOAD_IDi;
	if (ibuf_cat(e, id->id_buf) != 0)
		goto done;
	len = ibuf_size(id->id_buf);

	/* CERT payload */
	if ((sa->sa_stateinit & IKED_REQ_CERT) &&
	    (certid->id_type != IKEV2_CERT_NONE)) {
		if (ikev2_next_payload(pld, len,
		    IKEV2_PAYLOAD_CERT) == -1)
			goto done;
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((cert = ibuf_advance(e, sizeof(*cert))) == NULL)
			goto done;
		cert->cert_type = certid->id_type;
		if (ibuf_cat(e, certid->id_buf) != 0)
			goto done;
		len = ibuf_size(certid->id_buf) + sizeof(*cert);

		/* CERTREQ payload(s) */
		if ((len = ikev2_add_certreq(e, &pld,
		    len, env->sc_certreq, env->sc_certreqtype)) == -1)
			goto done;

		if (env->sc_certreqtype != pol->pol_certreqtype &&
		    (len = ikev2_add_certreq(e, &pld,
		    len, NULL, pol->pol_certreqtype)) == -1)
			goto done;
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_AUTH) == -1)
		goto done;

	/* AUTH payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((auth = ibuf_advance(e, sizeof(*auth))) == NULL)
		goto done;
	auth->auth_method = sa->sa_localauth.id_type;
	if (ibuf_cat(e, sa->sa_localauth.id_buf) != 0)
		goto done;
	len = ibuf_size(sa->sa_localauth.id_buf) + sizeof(*auth);

	/* CP payload */
	if (sa->sa_cp) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_CP) == -1)
			goto done;

		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((len = ikev2_add_cp(env, sa, e)) == -1)
			goto done;
	}

	/* compression */
	if ((pol->pol_flags & IKED_POLICY_IPCOMP) &&
	    (len = ikev2_add_ipcompnotify(env, e, &pld, len, sa)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_SA) == -1)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(env, sa, e, &pol->pol_proposals, 0,
	    sa->sa_hdr.sh_initiator, 0)) == -1)
		goto done;

	if ((len = ikev2_add_ts(e, &pld, len, sa, 0)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_IKE_AUTH, firstpayload, 0);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_init_done(struct iked *env, struct iked_sa *sa)
{
	int		 ret;

	if (!sa_stateok(sa, IKEV2_STATE_VALID))
		return (0);	/* ignored */

	ret = ikev2_childsa_negotiate(env, sa, &sa->sa_kex, &sa->sa_proposals,
	    sa->sa_hdr.sh_initiator, 0);
	if (ret == 0)
		ret = ikev2_childsa_enable(env, sa);
	if (ret == 0) {
		sa_state(env, sa, IKEV2_STATE_ESTABLISHED);
		timer_set(env, &sa->sa_timer, ikev2_ike_sa_alive, sa);
		timer_add(env, &sa->sa_timer, IKED_IKE_SA_ALIVE_TIMEOUT);
		timer_set(env, &sa->sa_rekey, ikev2_ike_sa_rekey, sa);
		if (sa->sa_policy->pol_rekey)
			timer_add(env, &sa->sa_rekey, sa->sa_policy->pol_rekey);
	}

	if (ret)
		ikev2_childsa_delete(env, sa, 0, 0, NULL, 1);
	return (ret);
}

int
ikev2_policy2id(struct iked_static_id *polid, struct iked_id *id, int srcid)
{
	struct ikev2_id		 hdr;
	struct iked_static_id	 localpid;
	char			 idstr[IKED_ID_SIZE];
	struct in_addr		 in4;
	struct in6_addr		 in6;
	X509_NAME		*name = NULL;
	u_int8_t		*p;
	int			 len;

	/* Fixup the local Id if not specified */
	if (srcid && polid->id_type == 0) {
		polid = &localpid;
		bzero(polid, sizeof(*polid));

		/* Create a default local ID based on our FQDN */
		polid->id_type = IKEV2_ID_FQDN;
		if (gethostname((char *)polid->id_data,
		    sizeof(polid->id_data)) != 0)
			return (-1);
		polid->id_offset = 0;
		polid->id_length =
		    strlen((char *)polid->id_data); /* excluding NUL */
	}

	if (!polid->id_length)
		return (-1);

	/* Create an IKEv2 ID payload */
	bzero(&hdr, sizeof(hdr));
	hdr.id_type = id->id_type = polid->id_type;
	id->id_offset = sizeof(hdr);

	if ((id->id_buf = ibuf_new(&hdr, sizeof(hdr))) == NULL)
		return (-1);

	switch (id->id_type) {
	case IKEV2_ID_IPV4:
		if (inet_pton(AF_INET, (char *)polid->id_data, &in4) != 1 ||
		    ibuf_add(id->id_buf, &in4, sizeof(in4)) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	case IKEV2_ID_IPV6:
		if (inet_pton(AF_INET6, (char *)polid->id_data, &in6) != 1 ||
		    ibuf_add(id->id_buf, &in6, sizeof(in6)) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	case IKEV2_ID_ASN1_DN:
		/* policy has ID in string-format, convert to ASN1 */
		if ((name = ca_x509_name_parse(polid->id_data)) == NULL ||
		    (len = i2d_X509_NAME(name, NULL)) < 0 ||
		    (p = ibuf_reserve(id->id_buf, len)) == NULL ||
		    (i2d_X509_NAME(name, &p)) < 0) {
			if (name)
				X509_NAME_free(name);
			ibuf_release(id->id_buf);
			return (-1);
		}
		X509_NAME_free(name);
		break;
	default:
		if (ibuf_add(id->id_buf,
		    polid->id_data, polid->id_length) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	}

	if (ikev2_print_id(id, idstr, sizeof(idstr)) == -1)
		return (-1);

	log_debug("%s: %s %s length %zu", __func__,
	    srcid ? "srcid" : "dstid",
	    idstr, ibuf_size(id->id_buf));

	return (0);
}

struct ike_header *
ikev2_add_header(struct ibuf *buf, struct iked_sa *sa,
    u_int32_t msgid, u_int8_t nextpayload,
    u_int8_t exchange, u_int8_t flags)
{
	struct ike_header	*hdr;

	if ((hdr = ibuf_advance(buf, sizeof(*hdr))) == NULL) {
		log_debug("%s: failed to add header", __func__);
		return (NULL);
	}

	hdr->ike_ispi = htobe64(sa->sa_hdr.sh_ispi);
	hdr->ike_rspi = htobe64(sa->sa_hdr.sh_rspi);
	hdr->ike_nextpayload = nextpayload;
	hdr->ike_version = IKEV2_VERSION;
	hdr->ike_exchange = exchange;
	hdr->ike_msgid = htobe32(msgid);
	hdr->ike_length = htobe32(sizeof(*hdr));
	hdr->ike_flags = flags;

	if (sa->sa_hdr.sh_initiator)
		hdr->ike_flags |= IKEV2_FLAG_INITIATOR;

	return (hdr);
}

int
ikev2_set_header(struct ike_header *hdr, size_t length)
{
	u_int32_t	 hdrlength = sizeof(*hdr) + length;

	if (hdrlength > UINT32_MAX) {
		log_debug("%s: message too long", __func__);
		return (-1);
	}

	hdr->ike_length = htobe32(sizeof(*hdr) + length);

	return (0);
}

struct ikev2_payload *
ikev2_add_payload(struct ibuf *buf)
{
	struct ikev2_payload	*pld;

	if ((pld = ibuf_advance(buf, sizeof(*pld))) == NULL) {
		log_debug("%s: failed to add payload", __func__);
		return (NULL);
	}

	pld->pld_nextpayload = IKEV2_PAYLOAD_NONE;
	pld->pld_length = sizeof(*pld);

	return (pld);
}

ssize_t
ikev2_add_ts_payload(struct ibuf *buf, u_int type, struct iked_sa *sa)
{
	struct iked_policy	*pol = sa->sa_policy;
	struct ikev2_tsp	*tsp;
	struct ikev2_ts		*ts;
	struct iked_flow	*flow;
	struct iked_addr	*addr;
	struct iked_addr	 pooladdr;
	u_int8_t		*ptr;
	size_t			 len = 0;
	u_int32_t		 av[4], bv[4], mv[4];
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;

	if ((tsp = ibuf_advance(buf, sizeof(*tsp))) == NULL)
		return (-1);
	tsp->tsp_count = pol->pol_nflows;
	len = sizeof(*tsp);

	RB_FOREACH(flow, iked_flows, &pol->pol_flows) {
		if ((ts = ibuf_advance(buf, sizeof(*ts))) == NULL)
			return (-1);

		if (type == IKEV2_PAYLOAD_TSi) {
			if (sa->sa_hdr.sh_initiator)
				addr = &flow->flow_src;
			else
				addr = &flow->flow_dst;
		} else if (type == IKEV2_PAYLOAD_TSr) {
			if (sa->sa_hdr.sh_initiator)
				addr = &flow->flow_dst;
			else
				addr = &flow->flow_src;
		} else
			return (-1);

		/* patch remote address (if configured to 0.0.0.0) */
		if ((type == IKEV2_PAYLOAD_TSi && !sa->sa_hdr.sh_initiator) ||
		    (type == IKEV2_PAYLOAD_TSr && sa->sa_hdr.sh_initiator)) {
			if (ikev2_cp_fixaddr(sa, addr, &pooladdr) != -1)
				addr = &pooladdr;
		}

		ts->ts_protoid = flow->flow_ipproto;

		if (addr->addr_port) {
			ts->ts_startport = addr->addr_port;
			ts->ts_endport = addr->addr_port;
		} else {
			ts->ts_startport = 0;
			ts->ts_endport = 0xffff;
		}

		switch (addr->addr_af) {
		case AF_INET:
			ts->ts_type = IKEV2_TS_IPV4_ADDR_RANGE;
			ts->ts_length = htobe16(sizeof(*ts) + 8);

			if ((ptr = ibuf_advance(buf, 8)) == NULL)
				return (-1);

			in4 = (struct sockaddr_in *)&addr->addr;
			if (addr->addr_net) {
				/* Convert IPv4 network to address range */
				mv[0] = prefixlen2mask(addr->addr_mask);
				av[0] = in4->sin_addr.s_addr & mv[0];
				bv[0] = in4->sin_addr.s_addr | ~mv[0];
			} else
				av[0] = bv[0] = in4->sin_addr.s_addr;

			memcpy(ptr, &av[0], 4);
			memcpy(ptr + 4, &bv[0], 4);
			break;
		case AF_INET6:
			ts->ts_type = IKEV2_TS_IPV6_ADDR_RANGE;
			ts->ts_length = htobe16(sizeof(*ts) + 32);

			if ((ptr = ibuf_advance(buf, 32)) == NULL)
				return (-1);

			in6 = (struct sockaddr_in6 *)&addr->addr;

			memcpy(&av, &in6->sin6_addr.s6_addr, 16);
			memcpy(&bv, &in6->sin6_addr.s6_addr, 16);
			if (addr->addr_net) {
				/* Convert IPv6 network to address range */
				prefixlen2mask6(addr->addr_mask, mv);
				av[0] &= mv[0];
				av[1] &= mv[1];
				av[2] &= mv[2];
				av[3] &= mv[3];
				bv[0] |= ~mv[0];
				bv[1] |= ~mv[1];
				bv[2] |= ~mv[2];
				bv[3] |= ~mv[3];
			}

			memcpy(ptr, &av, 16);
			memcpy(ptr + 16, &bv, 16);
			break;
		}

		len += betoh16(ts->ts_length);
	}

	return (len);
}

ssize_t
ikev2_add_ts(struct ibuf *e, struct ikev2_payload **pld, ssize_t len,
    struct iked_sa *sa, int reverse)
{
	if (ikev2_next_payload(*pld, len, IKEV2_PAYLOAD_TSi) == -1)
		return (-1);

	/* TSi payload */
	if ((*pld = ikev2_add_payload(e)) == NULL)
		return (-1);
	if ((len = ikev2_add_ts_payload(e, reverse ? IKEV2_PAYLOAD_TSr :
	    IKEV2_PAYLOAD_TSi, sa)) == -1)
		return (-1);

	if (ikev2_next_payload(*pld, len, IKEV2_PAYLOAD_TSr) == -1)
		return (-1);

	/* TSr payload */
	if ((*pld = ikev2_add_payload(e)) == NULL)
		return (-1);
	if ((len = ikev2_add_ts_payload(e, reverse ? IKEV2_PAYLOAD_TSi :
	    IKEV2_PAYLOAD_TSr, sa)) == -1)
		return (-1);

	return (len);
}


ssize_t
ikev2_add_certreq(struct ibuf *e, struct ikev2_payload **pld, ssize_t len,
    struct ibuf *certreq, u_int8_t type)
{
	struct ikev2_cert	*cert;

	if (type == IKEV2_CERT_NONE)
		return (len);

	if (ikev2_next_payload(*pld, len, IKEV2_PAYLOAD_CERTREQ) == -1)
		return (-1);

	/* CERTREQ payload */
	if ((*pld = ikev2_add_payload(e)) == NULL)
		return (-1);

	if ((cert = ibuf_advance(e, sizeof(*cert))) == NULL)
		return (-1);

	cert->cert_type = type;
	len = sizeof(*cert);

	if (certreq != NULL && cert->cert_type == IKEV2_CERT_X509_CERT) {
		if (ikev2_add_buf(e, certreq) == -1)
			return (-1);
		len += ibuf_size(certreq);
	}

	log_debug("%s: type %s length %zd", __func__,
	    print_map(type, ikev2_cert_map), len);

	return (len);
}

ssize_t
ikev2_add_ipcompnotify(struct iked *env, struct ibuf *e,
    struct ikev2_payload **pld, ssize_t len, struct iked_sa *sa)
{
	struct iked_childsa		 csa;
	struct ikev2_notify		*n;
	u_int8_t			*ptr;
	u_int16_t			 cpi;
	u_int32_t			 spi;
	u_int8_t			 transform;

	/* we only support deflate */
	transform = IKEV2_IPCOMP_DEFLATE;

	bzero(&csa, sizeof(csa));
	csa.csa_saproto = IKEV2_SAPROTO_IPCOMP;
	csa.csa_ikesa = sa;
	csa.csa_local = &sa->sa_peer;
	csa.csa_peer = &sa->sa_local;

	if (pfkey_sa_init(env->sc_pfkey, &csa, &spi) == -1)
		return (-1);
	/*
	 * We get spi == 0 if the kernel does not support IPcomp,
	 * so just return the length of the current payload.
	 */
	if (spi == 0)
		return (len);
	cpi = htobe16((u_int16_t)spi);
	if (*pld)
		if (ikev2_next_payload(*pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			return (-1);
	if ((*pld = ikev2_add_payload(e)) == NULL)
		return (-1);
	len = sizeof(*n) + sizeof(cpi) + sizeof(transform);
	if ((ptr = ibuf_advance(e, len)) == NULL)
		return (-1);
	n = (struct ikev2_notify *)ptr;
	n->n_protoid = 0;
	n->n_spisize = 0;
	n->n_type = htobe16(IKEV2_N_IPCOMP_SUPPORTED);
	ptr += sizeof(*n);
	memcpy(ptr, &cpi, sizeof(cpi));
	ptr += sizeof(cpi);
	memcpy(ptr, &transform, sizeof(transform));

	sa->sa_cpi_in = spi;	/* already on host byte order */
	log_debug("%s: sa_cpi_in 0x%04x", __func__, sa->sa_cpi_in);

	return (len);
}

int
ikev2_next_payload(struct ikev2_payload *pld, size_t length,
    u_int8_t nextpayload)
{
	size_t	 pldlength = sizeof(*pld) + length;

	if (pldlength > UINT16_MAX) {
		log_debug("%s: payload too long", __func__);
		return (-1);
	}

	log_debug("%s: length %zu nextpayload %s",
	    __func__, pldlength, print_map(nextpayload, ikev2_payload_map));

	pld->pld_length = htobe16(pldlength);
	pld->pld_nextpayload = nextpayload;

	return (0);
}

ssize_t
ikev2_nat_detection(struct iked *env, struct iked_message *msg,
    void *ptr, size_t len, u_int type)
{
	EVP_MD_CTX		 ctx;
	struct ike_header	*hdr;
	u_int8_t		 md[SHA_DIGEST_LENGTH];
	u_int			 mdlen = sizeof(md);
	struct iked_sa		*sa = msg->msg_sa;
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;
	ssize_t			 ret = -1;
	struct sockaddr		*src, *dst, *ss;
	u_int64_t		 rspi, ispi;
	struct ibuf		*buf;
	int			 frompeer = 0;
	u_int32_t		 rnd;

	if (ptr == NULL)
		return (mdlen);

	if (ikev2_msg_frompeer(msg)) {
		buf = msg->msg_parent->msg_data;
		if ((hdr = ibuf_seek(buf, 0, sizeof(*hdr))) == NULL)
			return (-1);
		ispi = hdr->ike_ispi;
		rspi = hdr->ike_rspi;
		frompeer = 1;
		src = (struct sockaddr *)&msg->msg_peer;
		dst = (struct sockaddr *)&msg->msg_local;
	} else {
		ispi = htobe64(sa->sa_hdr.sh_ispi);
		rspi = htobe64(sa->sa_hdr.sh_rspi);
		frompeer = 0;
		src = (struct sockaddr *)&msg->msg_local;
		dst = (struct sockaddr *)&msg->msg_peer;
	}

	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	switch (type) {
	case IKEV2_N_NAT_DETECTION_SOURCE_IP:
		log_debug("%s: %s source %s %s %s", __func__,
		    frompeer ? "peer" : "local",
		    print_spi(betoh64(ispi), 8),
		    print_spi(betoh64(rspi), 8),
		    print_host(src, NULL, 0));
		ss = src;
		break;
	case IKEV2_N_NAT_DETECTION_DESTINATION_IP:
		log_debug("%s: %s destination %s %s %s", __func__,
		    frompeer ? "peer" : "local",
		    print_spi(betoh64(ispi), 8),
		    print_spi(betoh64(rspi), 8),
		    print_host(dst, NULL, 0));
		ss = dst;
		break;
	default:
		goto done;
	}

	EVP_DigestUpdate(&ctx, &ispi, sizeof(ispi));
	EVP_DigestUpdate(&ctx, &rspi, sizeof(rspi));

	switch (ss->sa_family) {
	case AF_INET:
		in4 = (struct sockaddr_in *)ss;
		EVP_DigestUpdate(&ctx, &in4->sin_addr.s_addr,
		    sizeof(in4->sin_addr.s_addr));
		EVP_DigestUpdate(&ctx, &in4->sin_port,
		    sizeof(in4->sin_port));
		break;
	case AF_INET6:
		in6 = (struct sockaddr_in6 *)ss;
		EVP_DigestUpdate(&ctx, &in6->sin6_addr.s6_addr,
		    sizeof(in6->sin6_addr.s6_addr));
		EVP_DigestUpdate(&ctx, &in6->sin6_port,
		    sizeof(in6->sin6_port));
		break;
	default:
		goto done;
	}

	if (env->sc_opts & IKED_OPT_NATT) {
		/* Enforce NAT-T/UDP-encapsulation by distorting the digest */
		rnd = arc4random();
		EVP_DigestUpdate(&ctx, &rnd, sizeof(rnd));
	}

	EVP_DigestFinal_ex(&ctx, md, &mdlen);

	if (len < mdlen)
		goto done;

	memcpy(ptr, md, mdlen);
	ret = mdlen;
 done:
	EVP_MD_CTX_cleanup(&ctx);

	return (ret);
}

ssize_t
ikev2_add_cp(struct iked *env, struct iked_sa *sa, struct ibuf *buf)
{
	struct iked_policy	*pol = sa->sa_policy;
	struct ikev2_cp		*cp;
	struct ikev2_cfg	*cfg;
	struct iked_cfg		*ikecfg;
	u_int			 i;
	u_int32_t		 mask4;
	size_t			 len;
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;
	u_int8_t		 prefixlen;

	if ((cp = ibuf_advance(buf, sizeof(*cp))) == NULL)
		return (-1);
	len = sizeof(*cp);

	switch (sa->sa_cp) {
	case IKEV2_CP_REQUEST:
		cp->cp_type = IKEV2_CP_REPLY;
		break;
	case IKEV2_CP_REPLY:
	case IKEV2_CP_SET:
	case IKEV2_CP_ACK:
		/* Not yet supported */
		return (-1);
	}

	for (i = 0; i < pol->pol_ncfg; i++) {
		ikecfg = &pol->pol_cfg[i];
		if (ikecfg->cfg_action != cp->cp_type)
			continue;

		if ((cfg = ibuf_advance(buf, sizeof(*cfg))) == NULL)
			return (-1);

		cfg->cfg_type = htobe16(ikecfg->cfg_type);
		len += sizeof(*cfg);

		switch (ikecfg->cfg_type) {
		case IKEV2_CFG_INTERNAL_IP4_ADDRESS:
		case IKEV2_CFG_INTERNAL_IP4_NETMASK:
		case IKEV2_CFG_INTERNAL_IP4_DNS:
		case IKEV2_CFG_INTERNAL_IP4_NBNS:
		case IKEV2_CFG_INTERNAL_IP4_DHCP:
		case IKEV2_CFG_INTERNAL_IP4_SERVER:
			/* 4 bytes IPv4 address */
			in4 = (ikecfg->cfg.address.addr_mask != 32 &&
			    (ikecfg->cfg_type ==
			    IKEV2_CFG_INTERNAL_IP4_ADDRESS) &&
			    sa->sa_addrpool &&
			    sa->sa_addrpool->addr_af == AF_INET) ?
			    (struct sockaddr_in *)&sa->sa_addrpool->addr :
			    (struct sockaddr_in *)&ikecfg->cfg.address.addr;
			cfg->cfg_length = htobe16(4);
			if (ibuf_add(buf, &in4->sin_addr.s_addr, 4) == -1)
				return (-1);
			len += 4;
			break;
		case IKEV2_CFG_INTERNAL_IP4_SUBNET:
			/* 4 bytes IPv4 address + 4 bytes IPv4 mask + */
			in4 = (struct sockaddr_in *)&ikecfg->cfg.address.addr;
			mask4 = prefixlen2mask(ikecfg->cfg.address.addr_mask);
			cfg->cfg_length = htobe16(8);
			if (ibuf_add(buf, &in4->sin_addr.s_addr, 4) == -1)
				return (-1);
			if (ibuf_add(buf, &mask4, 4) == -1)
				return (-1);
			len += 8;
			break;
		case IKEV2_CFG_INTERNAL_IP6_DNS:
		case IKEV2_CFG_INTERNAL_IP6_NBNS:
		case IKEV2_CFG_INTERNAL_IP6_DHCP:
		case IKEV2_CFG_INTERNAL_IP6_SERVER:
			/* 16 bytes IPv6 address */
			in6 = (struct sockaddr_in6 *)&ikecfg->cfg.address;
			cfg->cfg_length = htobe16(16);
			if (ibuf_add(buf, &in6->sin6_addr.s6_addr, 16) == -1)
				return (-1);
			len += 16;
			break;
		case IKEV2_CFG_INTERNAL_IP6_ADDRESS:
		case IKEV2_CFG_INTERNAL_IP6_SUBNET:
			/* 16 bytes IPv6 address + 1 byte prefix length */
			in6 = (ikecfg->cfg.address.addr_mask != 128 &&
			    (ikecfg->cfg_type ==
			    IKEV2_CFG_INTERNAL_IP6_ADDRESS) &&
			    sa->sa_addrpool &&
			    sa->sa_addrpool->addr_af == AF_INET6) ?
			    (struct sockaddr_in6 *)&sa->sa_addrpool->addr :
			    (struct sockaddr_in6 *)&ikecfg->cfg.address.addr;
			cfg->cfg_length = htobe16(17);
			if (ibuf_add(buf, &in6->sin6_addr.s6_addr, 16) == -1)
				return (-1);
			if (ikecfg->cfg.address.addr_net)
				prefixlen = ikecfg->cfg.address.addr_mask;
			else
				prefixlen = 128;
			if (ibuf_add(buf, &prefixlen, 1) == -1)
				return (-1);
			len += 16 + 1;
			break;
		case IKEV2_CFG_APPLICATION_VERSION:
			/* Reply with an empty string (non-NUL terminated) */
			cfg->cfg_length = 0;
			break;
		}
	}

	return (len);
}

ssize_t
ikev2_add_proposals(struct iked *env, struct iked_sa *sa, struct ibuf *buf,
    struct iked_proposals *proposals, u_int8_t protoid, int initiator,
    int sendikespi)
{
	struct ikev2_sa_proposal	*sap;
	struct iked_transform		*xform;
	struct iked_proposal		*prop;
	struct iked_childsa		 csa;
	ssize_t				 length = 0, saplength, xflen;
	u_int64_t			 spi64;
	u_int32_t			 spi32, spi;
	u_int				 i;

	TAILQ_FOREACH(prop, proposals, prop_entry) {
		if ((protoid && prop->prop_protoid != protoid) ||
		    (!protoid && prop->prop_protoid == IKEV2_SAPROTO_IKE))
			continue;

		if (protoid != IKEV2_SAPROTO_IKE && initiator) {
			bzero(&csa, sizeof(csa));
			csa.csa_ikesa = sa;
			csa.csa_saproto = prop->prop_protoid;
			csa.csa_local = &sa->sa_peer;
			csa.csa_peer = &sa->sa_local;

			if (pfkey_sa_init(env->sc_pfkey, &csa, &spi) == -1)
				return (-1);

			prop->prop_localspi.spi = spi;
			prop->prop_localspi.spi_size = 4;
			prop->prop_localspi.spi_protoid = prop->prop_protoid;
		}

		if ((sap = ibuf_advance(buf, sizeof(*sap))) == NULL) {
			log_debug("%s: failed to add proposal", __func__);
			return (-1);
		}

		if (sendikespi) {
			/* Special case for IKE SA rekeying */
			prop->prop_localspi.spi = initiator ?
			    sa->sa_hdr.sh_ispi : sa->sa_hdr.sh_rspi;
			prop->prop_localspi.spi_size = 8;
			prop->prop_localspi.spi_protoid = IKEV2_SAPROTO_IKE;
		}

		sap->sap_proposalnr = prop->prop_id;
		sap->sap_protoid = prop->prop_protoid;
		sap->sap_spisize = prop->prop_localspi.spi_size;
		sap->sap_transforms = prop->prop_nxforms;
		saplength = sizeof(*sap);

		switch (prop->prop_localspi.spi_size) {
		case 4:
			spi32 = htobe32(prop->prop_localspi.spi);
			if (ibuf_add(buf, &spi32, sizeof(spi32)) != 0)
				return (-1);
			saplength += 4;
			break;
		case 8:
			spi64 = htobe64(prop->prop_localspi.spi);
			if (ibuf_add(buf, &spi64, sizeof(spi64)) != 0)
				return (-1);
			saplength += 8;
			break;
		default:
			break;
		}

		for (i = 0; i < prop->prop_nxforms; i++) {
			xform = prop->prop_xforms + i;

			if ((xflen = ikev2_add_transform(buf,
			    i == prop->prop_nxforms - 1 ?
			    IKEV2_XFORM_LAST : IKEV2_XFORM_MORE,
			    xform->xform_type, xform->xform_id,
			    xform->xform_length)) == -1)
				return (-1);

			saplength += xflen;
		}

		sap->sap_length = htobe16(saplength);
		length += saplength;
	}

	log_debug("%s: length %zd", __func__, length);

	return (length);
}

ssize_t
ikev2_add_transform(struct ibuf *buf,
    u_int8_t more, u_int8_t type, u_int16_t id, u_int16_t length)
{
	struct ikev2_transform	*xfrm;
	struct ikev2_attribute	*attr;

	if ((xfrm = ibuf_advance(buf, sizeof(*xfrm))) == NULL) {
		log_debug("%s: failed to add transform", __func__);
		return (-1);
	}
	xfrm->xfrm_more = more;
	xfrm->xfrm_type = type;
	xfrm->xfrm_id = htobe16(id);

	if (length) {
		xfrm->xfrm_length = htobe16(sizeof(*xfrm) + sizeof(*attr));

		if ((attr = ibuf_advance(buf, sizeof(*attr))) == NULL) {
			log_debug("%s: failed to add attribute", __func__);
			return (-1);
		}
		attr->attr_type = htobe16(IKEV2_ATTRAF_TV |
		    IKEV2_ATTRTYPE_KEY_LENGTH);
		attr->attr_length = htobe16(length);
	} else
		xfrm->xfrm_length = htobe16(sizeof(*xfrm));

	return (betoh16(xfrm->xfrm_length));
}

int
ikev2_add_data(struct ibuf *buf, void *data, size_t length)
{
	void	*msgbuf;

	if ((msgbuf = ibuf_advance(buf, length)) == NULL) {
		log_debug("%s: failed", __func__);
		return (-1);
	}
	memcpy(msgbuf, data, length);

	return (0);
}

int
ikev2_add_buf(struct ibuf *buf, struct ibuf *data)
{
	void	*msgbuf;

	if ((msgbuf = ibuf_advance(buf, ibuf_size(data))) == NULL) {
		log_debug("%s: failed", __func__);
		return (-1);
	}
	memcpy(msgbuf, ibuf_data(data), ibuf_size(data));

	return (0);
}

void
ikev2_resp_recv(struct iked *env, struct iked_message *msg,
    struct ike_header *hdr)
{
	struct iked_sa		*sa;

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		if (msg->msg_sa != NULL) {
			log_debug("%s: SA already exists", __func__);
			return;
		}
		if ((msg->msg_sa = sa_new(env,
		    betoh64(hdr->ike_ispi), betoh64(hdr->ike_rspi),
		    0, msg->msg_policy)) == NULL) {
			log_debug("%s: failed to get new SA", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
		if (ikev2_msg_valid_ike_sa(env, hdr, msg) == -1)
			return;
		if (sa_stateok(msg->msg_sa, IKEV2_STATE_VALID)) {
			log_debug("%s: already authenticated", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		if (ikev2_msg_valid_ike_sa(env, hdr, msg) == -1)
			return;
		break;
	case IKEV2_EXCHANGE_INFORMATIONAL:
		if (ikev2_msg_valid_ike_sa(env, hdr, msg) == -1)
			return;
		break;
	default:
		log_debug("%s: unsupported exchange: %s", __func__,
		    print_map(hdr->ike_exchange, ikev2_exchange_map));
		return;
	}

	if (ikev2_pld_parse(env, hdr, msg, msg->msg_offset) != 0) {
		log_debug("%s: failed to parse message", __func__);
		return;
	}

	if (!ikev2_msg_frompeer(msg))
		return;

	if ((sa = msg->msg_sa) == NULL)
		return;

	if (msg->msg_natt && sa->sa_natt == 0) {
		log_debug("%s: NAT-T message received, updated SA", __func__);
		sa->sa_natt = 1;
	}

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		if (ikev2_sa_responder(env, sa, NULL, msg) != 0) {
			log_debug("%s: failed to get IKE SA keys", __func__);
			sa_state(env, sa, IKEV2_STATE_CLOSED);
			return;
		}
		if (ikev2_resp_ike_sa_init(env, msg) != 0) {
			log_debug("%s: failed to send init response", __func__);
			sa_state(env, sa, IKEV2_STATE_CLOSED);
			return;
		}
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
		if (!sa_stateok(sa, IKEV2_STATE_SA_INIT)) {
			log_debug("%s: state mismatch", __func__);
			sa_state(env, sa, IKEV2_STATE_CLOSED);
			return;
		}

		if (!sa_stateok(sa, IKEV2_STATE_AUTH_REQUEST) &&
		    sa->sa_policy->pol_auth.auth_eap)
			sa_state(env, sa, IKEV2_STATE_EAP);

		if (ikev2_ike_auth_recv(env, sa, msg) != 0) {
			log_debug("%s: failed to send auth response", __func__);
			sa_state(env, sa, IKEV2_STATE_CLOSED);
			return;
		}
		break;
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		(void)ikev2_resp_create_child_sa(env, msg);
		break;
	case IKEV2_EXCHANGE_INFORMATIONAL:
		if (!msg->msg_responded && !msg->msg_error) {
			(void)ikev2_send_ike_e(env, sa, NULL,
			    IKEV2_PAYLOAD_NONE, IKEV2_EXCHANGE_INFORMATIONAL,
			    1);
			msg->msg_responded = 1;
		}
		break;
	default:
		break;
	}
}

int
ikev2_resp_ike_sa_init(struct iked *env, struct iked_message *msg)
{
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ikev2_keyexchange	*ke;
	struct ikev2_notify		*n;
	struct iked_sa			*sa = msg->msg_sa;
	struct ibuf			*buf;
	struct group			*group;
	u_int8_t			*ptr;
	ssize_t				 len;
	int				 ret = -1;

	if (sa->sa_hdr.sh_initiator) {
		log_debug("%s: called by initiator", __func__);
		return (-1);
	}

	if ((buf = ikev2_msg_init(env, &resp,
	    &msg->msg_peer, msg->msg_peerlen,
	    &msg->msg_local, msg->msg_locallen, 1)) == NULL)
		goto done;

	resp.msg_sa = sa;
	resp.msg_fd = msg->msg_fd;
	resp.msg_natt = msg->msg_natt;
	resp.msg_msgid = 0;

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, sa, resp.msg_msgid,
	    IKEV2_PAYLOAD_SA, IKEV2_EXCHANGE_IKE_SA_INIT,
	    IKEV2_FLAG_RESPONSE)) == NULL)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(env, sa, buf, &sa->sa_proposals,
	    IKEV2_SAPROTO_IKE, sa->sa_hdr.sh_initiator, 0)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_KE) == -1)
		goto done;

	/* KE payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((ke = ibuf_advance(buf, sizeof(*ke))) == NULL)
		goto done;
	if ((group = sa->sa_dhgroup) == NULL) {
		log_debug("%s: invalid dh", __func__);
		goto done;
	}
	ke->kex_dhgroup = htobe16(group->id);
	if (ikev2_add_buf(buf, sa->sa_dhrexchange) == -1)
		goto done;
	len = sizeof(*ke) + dh_getlen(group);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if (ikev2_add_buf(buf, sa->sa_rnonce) == -1)
		goto done;
	len = ibuf_size(sa->sa_rnonce);

	if ((env->sc_opts & IKED_OPT_NONATT) == 0 &&
	    msg->msg_local.ss_family != AF_UNSPEC) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		/* NAT-T notify payloads */
		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_NAT_DETECTION_SOURCE_IP);
		len = ikev2_nat_detection(env, &resp, NULL, 0, 0);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(env, &resp, ptr, len,
		    betoh16(n->n_type))) == -1)
			goto done;
		len += sizeof(*n);

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_NAT_DETECTION_DESTINATION_IP);
		len = ikev2_nat_detection(env, &resp, NULL, 0, 0);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(env, &resp, ptr, len,
		    betoh16(n->n_type))) == -1)
			goto done;
		len += sizeof(*n);
	}

	if (sa->sa_statevalid & IKED_REQ_CERT) {
		/* CERTREQ payload(s) */
		if ((len = ikev2_add_certreq(buf, &pld,
		    len, env->sc_certreq, env->sc_certreqtype)) == -1)
			goto done;

		if (env->sc_certreqtype != sa->sa_policy->pol_certreqtype &&
		    (len = ikev2_add_certreq(buf, &pld,
		    len, NULL, sa->sa_policy->pol_certreqtype)) == -1)
			goto done;
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	(void)ikev2_pld_parse(env, hdr, &resp, 0);

	ibuf_release(sa->sa_2ndmsg);
	if ((sa->sa_2ndmsg = ibuf_dup(buf)) == NULL) {
		log_debug("%s: failed to copy 2nd message", __func__);
		goto done;
	}

	resp.msg_sa = NULL;	/* Don't save the response */
	ret = ikev2_msg_send(env, &resp);

 done:
	ikev2_msg_cleanup(env, &resp);

	return (ret);
}

int
ikev2_resp_ike_auth(struct iked *env, struct iked_sa *sa)
{
	struct ikev2_payload		*pld;
	struct ikev2_notify		*n;
	struct ikev2_cert		*cert;
	struct ikev2_auth		*auth;
	struct iked_id			*id, *certid;
	struct ibuf			*e = NULL;
	u_int8_t			 firstpayload;
	int				 ret = -1;
	ssize_t				 len;

	if (sa == NULL)
		return (-1);

	if (sa->sa_state == IKEV2_STATE_EAP)
		return (ikev2_resp_ike_eap(env, sa, NULL));
	else if (!sa_stateok(sa, IKEV2_STATE_VALID))
		return (0);	/* ignore */

	if (ikev2_cp_setaddr(env, sa) < 0)
		return (-1);

	if (ikev2_childsa_negotiate(env, sa, &sa->sa_kex, &sa->sa_proposals,
	    sa->sa_hdr.sh_initiator, 0) < 0)
		return (-1);

	/* New encrypted message buffer */
	if ((e = ibuf_static()) == NULL)
		goto done;

	if (!sa->sa_localauth.id_type) {
		/* Downgrade the state */
		sa_state(env, sa, IKEV2_STATE_AUTH_SUCCESS);
	}

	if (!sa_stateok(sa, IKEV2_STATE_VALID)) {
		/* Notify payload */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		firstpayload = IKEV2_PAYLOAD_NOTIFY;

		if ((n = ibuf_advance(e, sizeof(*n))) == NULL)
			goto done;
		n->n_protoid = IKEV2_SAPROTO_IKE;	/* XXX ESP etc. */
		n->n_spisize = 0;
		n->n_type = htobe16(IKEV2_N_AUTHENTICATION_FAILED);
		len = sizeof(*n);

		goto send;
	}

	if (sa->sa_hdr.sh_initiator) {
		id = &sa->sa_iid;
		certid = &sa->sa_icert;
	} else {
		id = &sa->sa_rid;
		certid = &sa->sa_rcert;
	}

	if (sa->sa_state != IKEV2_STATE_EAP_VALID) {
		/* ID payload */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		firstpayload = IKEV2_PAYLOAD_IDr;
		if (ibuf_cat(e, id->id_buf) != 0)
			goto done;
		len = ibuf_size(id->id_buf);

		/* CERT payload */
		if ((sa->sa_statevalid & IKED_REQ_CERT) &&
		    (certid->id_type != IKEV2_CERT_NONE)) {
			if (ikev2_next_payload(pld, len,
			    IKEV2_PAYLOAD_CERT) == -1)
				goto done;

			if ((pld = ikev2_add_payload(e)) == NULL)
				goto done;
			if ((cert = ibuf_advance(e, sizeof(*cert))) == NULL)
				goto done;
			cert->cert_type = certid->id_type;
			if (ibuf_cat(e, certid->id_buf) != 0)
				goto done;
			len = ibuf_size(certid->id_buf) + sizeof(*cert);
		}

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_AUTH) == -1)
			goto done;
	} else
		firstpayload = IKEV2_PAYLOAD_AUTH;

	/* AUTH payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((auth = ibuf_advance(e, sizeof(*auth))) == NULL)
		goto done;
	auth->auth_method = sa->sa_localauth.id_type;
	if (ibuf_cat(e, sa->sa_localauth.id_buf) != 0)
		goto done;
	len = ibuf_size(sa->sa_localauth.id_buf) + sizeof(*auth);

	/* CP payload */
	if (sa->sa_cp) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_CP) == -1)
			goto done;

		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((len = ikev2_add_cp(env, sa, e)) == -1)
			goto done;
	}

	/* compression */
	if (sa->sa_ipcomp &&
	    (len = ikev2_add_ipcompnotify(env, e, &pld, len, sa)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_SA) == -1)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(env, sa, e, &sa->sa_proposals, 0,
	    sa->sa_hdr.sh_initiator, 0)) == -1)
		goto done;

	if ((len = ikev2_add_ts(e, &pld, len, sa, 0)) == -1)
		goto done;

 send:
	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_IKE_AUTH, firstpayload, 1);
	if (ret == 0)
		ret = ikev2_childsa_enable(env, sa);
	if (ret == 0) {
		sa_state(env, sa, IKEV2_STATE_ESTABLISHED);
		timer_set(env, &sa->sa_timer, ikev2_ike_sa_alive, sa);
		timer_add(env, &sa->sa_timer, IKED_IKE_SA_ALIVE_TIMEOUT);
		timer_set(env, &sa->sa_rekey, ikev2_ike_sa_rekey, sa);
		if (sa->sa_policy->pol_rekey)
			timer_add(env, &sa->sa_rekey, sa->sa_policy->pol_rekey);
	}

 done:
	if (ret)
		ikev2_childsa_delete(env, sa, 0, 0, NULL, 1);
	ibuf_release(e);
	return (ret);
}

int
ikev2_resp_ike_eap(struct iked *env, struct iked_sa *sa, struct ibuf *eapmsg)
{
	struct ikev2_payload		*pld;
	struct ikev2_cert		*cert;
	struct ikev2_auth		*auth;
	struct iked_id			*id, *certid;
	struct ibuf			*e = NULL;
	u_int8_t			 firstpayload;
	int				 ret = -1;
	ssize_t				 len = 0;

	/* Responder only */
	if (sa->sa_hdr.sh_initiator)
		return (-1);

	/* Check if "ca" has done it's job yet */
	if (!sa->sa_localauth.id_type)
		return (0);

	/* New encrypted message buffer */
	if ((e = ibuf_static()) == NULL)
		goto done;

	id = &sa->sa_rid;
	certid = &sa->sa_rcert;

	/* ID payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	firstpayload = IKEV2_PAYLOAD_IDr;
	if (ibuf_cat(e, id->id_buf) != 0)
		goto done;
	len = ibuf_size(id->id_buf);

	if ((sa->sa_statevalid & IKED_REQ_CERT) &&
	    (certid->id_type != IKEV2_CERT_NONE)) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_CERT) == -1)
			goto done;

		/* CERT payload */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((cert = ibuf_advance(e, sizeof(*cert))) == NULL)
			goto done;
		cert->cert_type = certid->id_type;
		if (ibuf_cat(e, certid->id_buf) != 0)
			goto done;
		len = ibuf_size(certid->id_buf) + sizeof(*cert);
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_AUTH) == -1)
		goto done;

	/* AUTH payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((auth = ibuf_advance(e, sizeof(*auth))) == NULL)
		goto done;
	auth->auth_method = sa->sa_localauth.id_type;
	if (ibuf_cat(e, sa->sa_localauth.id_buf) != 0)
		goto done;
	len = ibuf_size(sa->sa_localauth.id_buf) + sizeof(*auth);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_EAP) == -1)
		goto done;

	/* EAP payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = eap_identity_request(e)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_IKE_AUTH, firstpayload, 1);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_send_ike_e(struct iked *env, struct iked_sa *sa, struct ibuf *buf,
    u_int8_t firstpayload, u_int8_t exchange, int response)
{
	struct ikev2_payload		*pld;
	struct ibuf			*e = NULL;
	int				 ret = -1;

	/* New encrypted message buffer */
	if ((e = ibuf_static()) == NULL)
		goto done;

	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;

	if (buf) {
		if (ibuf_cat(e, buf) != 0)
			goto done;

		if (ikev2_next_payload(pld, ibuf_size(buf),
		    IKEV2_PAYLOAD_NONE) == -1)
			goto done;
	}

	ret = ikev2_msg_send_encrypt(env, sa, &e, exchange, firstpayload,
	    response);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_set_sa_proposal(struct iked_sa *sa, struct iked_policy *pol,
    u_int proto)
{
	struct iked_proposal	 	*prop, *copy;
	struct iked_transform	 	*xform;
	u_int			 	 i;

	/* create copy of the policy proposals */
	config_free_proposals(&sa->sa_proposals, proto);
	TAILQ_FOREACH(prop, &pol->pol_proposals, prop_entry) {
		if (proto != 0 && prop->prop_protoid != proto)
			continue;
		if ((copy = config_add_proposal(&sa->sa_proposals,
		    prop->prop_id, prop->prop_protoid)) == NULL)
			return (-1);
		for (i = 0; i < prop->prop_nxforms; i++) {
			xform = &prop->prop_xforms[i];
			if (config_add_transform(copy, xform->xform_type,
			    xform->xform_id, xform->xform_length,
			    xform->xform_keylength) == NULL)
				return (-1);
		}
	}
	return (0);
}

int
ikev2_send_create_child_sa(struct iked *env, struct iked_sa *sa,
    struct iked_spi *rekey, u_int8_t protoid)
{
	struct iked_policy		*pol = sa->sa_policy;
	struct iked_childsa		*csa = NULL, *csb = NULL;
	struct ikev2_notify		*n;
	struct ikev2_payload		*pld = NULL;
	struct ikev2_keyexchange	*ke;
	struct group			*group;
	struct ibuf			*e = NULL, *nonce = NULL;
	u_int8_t			*ptr;
	u_int8_t			 firstpayload;
	u_int32_t			 spi;
	ssize_t				 len = 0;
	int				 initiator, ret = -1;

	if (rekey)
		log_debug("%s: rekeying %s spi %s", __func__,
		    print_map(rekey->spi_protoid, ikev2_saproto_map),
		    print_spi(rekey->spi, rekey->spi_size));
	else
		log_debug("%s: creating new CHILD SAs", __func__);

	/* XXX cannot initiate multiple concurrent CREATE_CHILD_SA exchanges */
	if (sa->sa_stateflags & IKED_REQ_CHILDSA) {
		log_debug("%s: another CREATE_CHILD_SA exchange already active",
		    __func__);
		return (-1);
	}

	sa->sa_rekeyspi = 0;	/* clear rekey spi */
	initiator = sa->sa_hdr.sh_initiator ? 1 : 0;

	if (rekey &&
	    ((csa = childsa_lookup(sa, rekey->spi,
	    rekey->spi_protoid)) == NULL ||
	    (csb = csa->csa_peersa) == NULL)) {
		log_debug("%s: CHILD SA %s wasn't found", __func__,
		    print_spi(rekey->spi, rekey->spi_size));
		goto done;
	}

	/* Generate new nonce */
	if ((nonce = ibuf_random(IKED_NONCE_SIZE)) == NULL)
		goto done;

	/* Update initiator nonce */
	ibuf_release(sa->sa_inonce);
	sa->sa_inonce = nonce;

	if ((e = ibuf_static()) == NULL)
		goto done;

	/* compression */
	if ((pol->pol_flags & IKED_POLICY_IPCOMP) &&
	    (len = ikev2_add_ipcompnotify(env, e, &pld, 0, sa)) == -1)
		goto done;

	if (pld) {
		firstpayload = IKEV2_PAYLOAD_NOTIFY;
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_SA) == -1)
			goto done;
	} else
		firstpayload = IKEV2_PAYLOAD_SA;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;

	/*
	 * We need to reset the sa_proposal. Otherwise it would be
	 * left over from the IKE_AUTH exchange and would not contain
	 * any DH groups (e.g. for ESP child SAs).
	 */
	if (ikev2_set_sa_proposal(sa, pol, protoid) < 0) {
		log_debug("%s: ikev2_set_sa_proposal failed", __func__);
		goto done;
	}

	if ((len = ikev2_add_proposals(env, sa, e, &sa->sa_proposals,
	    protoid, 1, 0)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if (ikev2_add_buf(e, nonce) == -1)
		goto done;
	len = ibuf_size(nonce);

	if (config_findtransform(&pol->pol_proposals, IKEV2_XFORMTYPE_DH,
	    protoid)) {
		log_debug("%s: enable PFS", __func__);
		ikev2_sa_cleanup_dh(sa);
		if (ikev2_sa_initiator_dh(sa, NULL, protoid) < 0) {
			log_debug("%s: failed to setup DH", __func__);
			goto done;
		}
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_KE) == -1)
			goto done;

		/* KE payload */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((ke = ibuf_advance(e, sizeof(*ke))) == NULL)
			goto done;
		if ((group = sa->sa_dhgroup) == NULL) {
			log_debug("%s: invalid dh", __func__);
			goto done;
		}
		ke->kex_dhgroup = htobe16(group->id);
		if (ikev2_add_buf(e, sa->sa_dhiexchange) == -1)
			goto done;
		len = sizeof(*ke) + dh_getlen(group);
	}

	if ((len = ikev2_add_ts(e, &pld, len, sa, !initiator)) == -1)
		goto done;

	if (rekey) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		/* REKEY_SA notification */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((n = ibuf_advance(e, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_REKEY_SA);
		n->n_protoid = rekey->spi_protoid;
		n->n_spisize = rekey->spi_size;
		if ((ptr = ibuf_advance(e, rekey->spi_size)) == NULL)
			goto done;
		len = rekey->spi_size;
		spi = htobe32((u_int32_t)csa->csa_peerspi);
		memcpy(ptr, &spi, rekey->spi_size);
		len += sizeof(*n);
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_CREATE_CHILD_SA, firstpayload, 0);
	if (ret == 0) {
		if (rekey) {
			csa->csa_rekey = 1;
			csb->csa_rekey = 1;
			/*
			 * Remember the peer spi of the rekeyed
			 * SA for ikev2_init_create_child_sa().
			 */
			sa->sa_rekeyspi = csa->csa_peerspi;
		}
		sa->sa_stateflags |= IKED_REQ_CHILDSA;
	}

done:
	ibuf_release(e);
	return (ret);
}

void
ikev2_ike_sa_rekey(struct iked *env, void *arg)
{
	struct iked_sa			*sa = arg;
	struct iked_sa			*nsa = NULL;
	struct ikev2_payload		*pld = NULL;
	struct ikev2_keyexchange	*ke;
	struct group			*group;
	struct ibuf			*e = NULL, *nonce = NULL;
	ssize_t				 len = 0;
	int				 ret = -1;

	log_debug("%s: called for IKE SA %p", __func__, sa);

	if (sa->sa_stateflags & IKED_REQ_CHILDSA) {
		/*
		 * We cannot initiate multiple concurrent CREATE_CHILD_SA
		 * exchanges, so retry in one minute.
		 */
		timer_add(env, &sa->sa_rekey, 60);
		return;
	}

	if ((nsa = sa_new(env, 0, 0, 1, sa->sa_policy)) == NULL) {
		log_debug("%s: failed to get new SA", __func__);
		goto done;
	}

	if (ikev2_sa_initiator(env, nsa, sa, NULL)) {
		log_debug("%s: failed to setup DH", __func__);
		goto done;
	}
	sa_state(env, nsa, IKEV2_STATE_AUTH_SUCCESS);
	nonce = nsa->sa_inonce;

	if ((e = ibuf_static()) == NULL)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;

	/* just reuse the old IKE SA proposals */
	if ((len = ikev2_add_proposals(env, nsa, e, &sa->sa_proposals,
	    IKEV2_SAPROTO_IKE, 1, 1)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if (ikev2_add_buf(e, nonce) == -1)
		goto done;
	len = ibuf_size(nonce);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_KE) == -1)
		goto done;

	/* KE payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((ke = ibuf_advance(e, sizeof(*ke))) == NULL)
		goto done;
	if ((group = nsa->sa_dhgroup) == NULL) {
		log_debug("%s: invalid dh", __func__);
		goto done;
	}
	ke->kex_dhgroup = htobe16(group->id);
	if (ikev2_add_buf(e, nsa->sa_dhiexchange) == -1)
		goto done;
	len = sizeof(*ke) + dh_getlen(group);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_CREATE_CHILD_SA, IKEV2_PAYLOAD_SA, 0);
	if (ret == 0) {
		sa->sa_stateflags |= IKED_REQ_CHILDSA;
		sa->sa_next = nsa;
		nsa = NULL;
	}
done:
	if (nsa)
		sa_free(env, nsa);
	ibuf_release(e);

	if (ret == 0)
		log_debug("%s: create child SA sent", __func__);
	else
		log_debug("%s: could not send create child SA", __func__);
	/* XXX should we try again in case of ret != 0 ? */
}

int
ikev2_init_create_child_sa(struct iked *env, struct iked_message *msg)
{
	struct iked_childsa		*csa = NULL;
	struct iked_proposal		*prop;
	struct iked_sa			*sa = msg->msg_sa;
	struct iked_sa			*nsa;
	struct iked_spi			*spi;
	struct ikev2_delete		*del;
	struct ibuf			*buf = NULL;
	u_int32_t			 spi32;
	int				 pfs = 0, ret = -1;

	if (!ikev2_msg_frompeer(msg) ||
	    (sa->sa_stateflags & IKED_REQ_CHILDSA) == 0)
		return (0);

	if (msg->msg_prop == NULL ||
	    TAILQ_EMPTY(&msg->msg_proposals)) {
		log_debug("%s: no proposal specified", __func__);
		return (-1);
	}

	if (ikev2_sa_negotiate(&sa->sa_proposals, &sa->sa_proposals,
	    &msg->msg_proposals) != 0) {
		log_debug("%s: no proposal chosen", __func__);
		return (-1);
	}

	TAILQ_FOREACH(prop, &sa->sa_proposals, prop_entry) {
		if (prop->prop_protoid == msg->msg_prop->prop_protoid)
			break;
	}
	if (prop == NULL) {
		log_debug("%s: failed to find %s proposals", __func__,
		    print_map(msg->msg_prop->prop_protoid, ikev2_saproto_map));
		return (-1);
	}

	/* IKE SA rekeying */
	if (prop->prop_protoid == IKEV2_SAPROTO_IKE) {
		if (sa->sa_next == NULL) {
			log_debug("%s: missing IKE SA for rekeying", __func__);
			return (-1);
		}
		/* Update the responder SPI */
		spi = &msg->msg_prop->prop_peerspi;
		if ((nsa = sa_new(env, sa->sa_next->sa_hdr.sh_ispi,
		    spi->spi, 1, NULL)) == NULL || nsa != sa->sa_next) {
			log_debug("%s: invalid rekey SA", __func__);
			if (nsa)
				sa_free(env, nsa);
			sa_free(env, sa->sa_next);
			sa->sa_next = NULL;
			return (-1);
		}
		if (ikev2_sa_initiator(env, nsa, sa, msg) == -1) {
			log_debug("%s: failed to get IKE keys", __func__);
			return (-1);
		}
		sa->sa_stateflags &= ~IKED_REQ_CHILDSA;
		sa->sa_next = NULL;
		return (ikev2_ikesa_enable(env, sa, nsa));
	}

	/* Child SA rekeying */
	if (sa->sa_rekeyspi &&
	    (csa = childsa_lookup(sa, sa->sa_rekeyspi, prop->prop_protoid))
	    != NULL) {
		log_debug("%s: rekeying CHILD SA old %s spi %s", __func__,
		    print_spi(csa->csa_spi.spi, csa->csa_spi.spi_size),
		    print_spi(prop->prop_peerspi.spi,
		    prop->prop_peerspi.spi_size));
	}

	/* check KE payload for PFS */
	if (ibuf_length(msg->msg_ke)) {
		log_debug("%s: using PFS", __func__);
		if (ikev2_sa_initiator_dh(sa, msg, prop->prop_protoid) < 0) {
			log_debug("%s: failed to setup DH", __func__);
			return (ret);
		}
		if (sa->sa_dhpeer == NULL) {
			log_debug("%s: no peer DH", __func__);
			return (ret);
		}
		pfs = 1;
		/* XXX check group against policy ? */
		/* XXX should ikev2_sa_negotiate do this? */
	}

	/* Update responder's nonce */
	if (!ibuf_length(msg->msg_nonce)) {
		log_debug("%s: responder didn't send nonce", __func__);
		return (-1);
	}
	ibuf_release(sa->sa_rnonce);
	sa->sa_rnonce = ibuf_dup(msg->msg_nonce);

	if (ikev2_childsa_negotiate(env, sa, &sa->sa_kex, &sa->sa_proposals, 1,
	    pfs)) {
		log_debug("%s: failed to get CHILD SAs", __func__);
		return (-1);
	}

	if (csa) {
		/* Child SA rekeying */

		if ((buf = ibuf_static()) == NULL)
			goto done;

		if ((del = ibuf_advance(buf, sizeof(*del))) == NULL)
			goto done;

		del->del_protoid = prop->prop_protoid;
		del->del_spisize = sizeof(spi32);
		del->del_nspi = htobe16(1);

		spi32 = htobe32(csa->csa_spi.spi);
		if (ibuf_add(buf, &spi32, sizeof(spi32)))
			goto done;

		if (ikev2_send_ike_e(env, sa, buf, IKEV2_PAYLOAD_DELETE,
		    IKEV2_EXCHANGE_INFORMATIONAL, 0))
			goto done;

		sa->sa_stateflags |= IKED_REQ_INF;
	}

	ret = ikev2_childsa_enable(env, sa);

done:
	sa->sa_stateflags &= ~IKED_REQ_CHILDSA;

	if (ret)
		ikev2_childsa_delete(env, sa, 0, 0, NULL, 1);
	ibuf_release(buf);
	return (ret);
}

int
ikev2_ikesa_enable(struct iked *env, struct iked_sa *sa, struct iked_sa *nsa)
{
	struct iked_childsa		*csa, *nextcsa;
	struct iked_flow		*flow, *nextflow;
	struct iked_proposal		*prop, *nextprop;
	int				 initiator;

	log_debug("%s: IKE SA %p ispi %s rspi %s replaced"
	    " by SA %p ispi %s rspi %s ",
	    __func__, sa,
	    print_spi(sa->sa_hdr.sh_ispi, 8),
	    print_spi(sa->sa_hdr.sh_rspi, 8),
	    nsa,
	    print_spi(nsa->sa_hdr.sh_ispi, 8),
	    print_spi(nsa->sa_hdr.sh_rspi, 8));

	/*
	 * Transfer policy and address:
	 * - Remember if we initiated the original IKE-SA because of our policy.
	 * - Note that sa_address() will insert the new SA when we set sa_peer.
	 */
	initiator = !memcmp(&sa->sa_polpeer, &sa->sa_policy->pol_peer,
	    sizeof(sa->sa_polpeer));
	nsa->sa_policy = sa->sa_policy;
	RB_REMOVE(iked_sapeers, &sa->sa_policy->pol_sapeers, sa);
	sa->sa_policy = NULL;
	if (sa_address(nsa, &nsa->sa_peer, &sa->sa_peer.addr,
	    initiator) == -1 ||
	    sa_address(nsa, &nsa->sa_local, &sa->sa_local.addr,
	    initiator) == -1) {
		/* reinsert old SA :/ */
		sa->sa_policy = nsa->sa_policy;
		if (RB_FIND(iked_sapeers, &nsa->sa_policy->pol_sapeers, nsa))
			RB_REMOVE(iked_sapeers, &nsa->sa_policy->pol_sapeers, nsa);
		RB_INSERT(iked_sapeers, &sa->sa_policy->pol_sapeers, sa);
		nsa->sa_policy = NULL;
		return (-1);
	}

	/* Transfer socket and NAT information */
	nsa->sa_fd = sa->sa_fd;
	nsa->sa_natt = sa->sa_natt;
	nsa->sa_udpencap = sa->sa_udpencap;

	/* Transfer all Child SAs and flows from the old IKE SA */
	for (flow = TAILQ_FIRST(&sa->sa_flows); flow != NULL;
	     flow = nextflow) {
		nextflow = TAILQ_NEXT(flow, flow_entry);
		TAILQ_REMOVE(&sa->sa_flows, flow, flow_entry);
		TAILQ_INSERT_TAIL(&nsa->sa_flows, flow,
		    flow_entry);
		flow->flow_ikesa = nsa;
		flow->flow_local = &nsa->sa_local;
		flow->flow_peer = &nsa->sa_peer;
	}
	for (csa = TAILQ_FIRST(&sa->sa_childsas); csa != NULL;
	     csa = nextcsa) {
		nextcsa = TAILQ_NEXT(csa, csa_entry);
		TAILQ_REMOVE(&sa->sa_childsas, csa, csa_entry);
		TAILQ_INSERT_TAIL(&nsa->sa_childsas, csa,
		    csa_entry);
		csa->csa_ikesa = nsa;
		if (csa->csa_dir == IPSP_DIRECTION_IN) {
			csa->csa_local = &nsa->sa_peer;
			csa->csa_peer = &nsa->sa_local;
		} else {
			csa->csa_local = &nsa->sa_local;
			csa->csa_peer = &nsa->sa_peer;
		}
	}
	/* Transfer all non-IKE proposals */
	for (prop = TAILQ_FIRST(&sa->sa_proposals); prop != NULL;
	     prop = nextprop) {
		nextprop = TAILQ_NEXT(prop, prop_entry);
		if (prop->prop_protoid == IKEV2_SAPROTO_IKE)
			continue;
		TAILQ_REMOVE(&sa->sa_proposals, prop, prop_entry);
		TAILQ_INSERT_TAIL(&nsa->sa_proposals, prop,
		    prop_entry);
	}

	/* Preserve ID information */
	if (sa->sa_hdr.sh_initiator == nsa->sa_hdr.sh_initiator) {
		nsa->sa_iid = sa->sa_iid;
		nsa->sa_rid = sa->sa_rid;
	} else {
		/* initiator and responder role swapped */
		nsa->sa_iid = sa->sa_rid;
		nsa->sa_rid = sa->sa_iid;
	}
	/* duplicate the actual buffer */
	nsa->sa_iid.id_buf = ibuf_dup(nsa->sa_iid.id_buf);
	nsa->sa_rid.id_buf = ibuf_dup(nsa->sa_rid.id_buf);

	/* Transfer sa_addrpool address */
	if (sa->sa_addrpool) {
		RB_REMOVE(iked_addrpool, &env->sc_addrpool, sa);
		nsa->sa_addrpool = sa->sa_addrpool;
		sa->sa_addrpool = NULL;
		RB_INSERT(iked_addrpool, &env->sc_addrpool, nsa);
	}

	log_debug("%s: activating new IKE SA", __func__);
	sa_state(env, nsa, IKEV2_STATE_ESTABLISHED);
	timer_set(env, &nsa->sa_timer, ikev2_ike_sa_alive, nsa);
	timer_add(env, &nsa->sa_timer, IKED_IKE_SA_ALIVE_TIMEOUT);
	timer_set(env, &nsa->sa_rekey, ikev2_ike_sa_rekey, nsa);
	if (nsa->sa_policy->pol_rekey)
		timer_add(env, &nsa->sa_rekey, nsa->sa_policy->pol_rekey);
	nsa->sa_stateflags = nsa->sa_statevalid; /* XXX */

	/* unregister DPD keep alive timer & rekey first */
	if (sa->sa_state == IKEV2_STATE_ESTABLISHED) {
		timer_del(env, &sa->sa_rekey);
		timer_del(env, &sa->sa_timer);
	}

	ikev2_ikesa_delete(env, sa, nsa->sa_hdr.sh_initiator);
	return (0);
}

void
ikev2_ikesa_delete(struct iked *env, struct iked_sa *sa, int initiator)
{
	struct ibuf                     *buf = NULL;
	struct ikev2_delete             *del;

	if (initiator) {
		/* Send PAYLOAD_DELETE */
		if ((buf = ibuf_static()) == NULL)
			goto done;
		if ((del = ibuf_advance(buf, sizeof(*del))) == NULL)
			goto done;
		del->del_protoid = IKEV2_SAPROTO_IKE;
		del->del_spisize = 0;
		del->del_nspi = 0;
		if (ikev2_send_ike_e(env, sa, buf, IKEV2_PAYLOAD_DELETE,
		    IKEV2_EXCHANGE_INFORMATIONAL, 0) == -1)
			goto done;
		log_debug("%s: sent delete, closing SA", __func__);
done:
		ibuf_release(buf);
		sa_state(env, sa, IKEV2_STATE_CLOSED);
	} else {
		sa_state(env, sa, IKEV2_STATE_CLOSING);
	}

	/* Remove IKE-SA after timeout, e.g. if we don't get a delete */
	timer_set(env, &sa->sa_timer, ikev2_ike_sa_timeout, sa);
	timer_add(env, &sa->sa_timer, IKED_IKE_SA_DELETE_TIMEOUT);
}

int
ikev2_resp_create_child_sa(struct iked *env, struct iked_message *msg)
{
	struct iked_childsa		*csa;
	struct iked_proposal		*prop;
	struct iked_proposals		 proposals;
	struct iked_kex		 	*kex, *kextmp = NULL;
	struct iked_sa			*nsa = NULL, *sa = msg->msg_sa;
	struct iked_spi			*spi, *rekey = &msg->msg_rekey;
	struct ikev2_keyexchange	*ke;
	struct ikev2_payload		*pld = NULL;
	struct ibuf			*e = NULL, *nonce = NULL;
	u_int8_t			 firstpayload;
	ssize_t				 len = 0;
	int				 initiator, protoid, rekeying = 1;
	int				 ret = -1;
	int				 pfs = 0;

	initiator = sa->sa_hdr.sh_initiator ? 1 : 0;

	if (!ikev2_msg_frompeer(msg) || msg->msg_prop == NULL)
		return (0);

	TAILQ_INIT(&proposals);

	if ((protoid = rekey->spi_protoid) == 0) {
		/*
		 * If REKEY_SA notification is not present, then it's either
		 * IKE SA rekeying or the client wants to create additional
		 * CHILD SAs
		 */
		if (msg->msg_prop->prop_protoid == IKEV2_SAPROTO_IKE) {
			protoid = rekey->spi_protoid = IKEV2_SAPROTO_IKE;
			if (sa->sa_hdr.sh_initiator)
				rekey->spi = sa->sa_hdr.sh_rspi;
			else
				rekey->spi = sa->sa_hdr.sh_ispi;
			rekey->spi_size = 8;
		} else {
			protoid = msg->msg_prop->prop_protoid;
			rekeying = 0;
		}
	}

	if (rekeying)
		log_debug("%s: rekey %s spi %s", __func__,
		    print_map(rekey->spi_protoid, ikev2_saproto_map),
		    print_spi(rekey->spi, rekey->spi_size));
	else
		log_debug("%s: creating new %s SA", __func__,
		    print_map(protoid, ikev2_saproto_map));

	if (protoid == IKEV2_SAPROTO_IKE) {
		/* IKE SA rekeying */
		spi = &msg->msg_prop->prop_peerspi;

		if ((nsa = sa_new(env, spi->spi, 0, 0,
		    msg->msg_policy)) == NULL) {
			log_debug("%s: failed to get new SA", __func__);
			return (ret);
		}

		if (ikev2_sa_responder(env, nsa, sa, msg)) {
			log_debug("%s: failed to get IKE SA keys", __func__);
			return (ret);
		}

		sa_state(env, nsa, IKEV2_STATE_AUTH_SUCCESS);

		nonce = nsa->sa_rnonce;
		kex = &nsa->sa_kex;
	} else {
		/* Child SA creating/rekeying */

		if ((kex = kextmp = calloc(1, sizeof(*kextmp))) == NULL) {
			log_debug("%s: calloc kex", __func__);
			goto fail;
		}

		if (ikev2_sa_negotiate(&proposals,
		    &sa->sa_policy->pol_proposals, &msg->msg_proposals) != 0) {
			log_debug("%s: no proposal chosen", __func__);
			goto fail;
		}

		/* check KE payload for PFS */
		if (ibuf_length(msg->msg_parent->msg_ke)) {
			log_debug("%s: using PFS", __func__);
			if (ikev2_sa_responder_dh(kex, &proposals,
			    msg->msg_parent, protoid) < 0) {
				log_debug("%s: failed to setup DH", __func__);
				goto fail;
			}
			pfs = 1;
			/* XXX check group against policy ? */
		}

		/* Update peer SPI */
		TAILQ_FOREACH(prop, &proposals, prop_entry) {
			if (prop->prop_protoid == protoid)
				break;
		}
		if (prop == NULL) {
			log_debug("%s: failed to find %s proposals", __func__,
			    print_map(protoid, ikev2_saproto_map));
			goto fail;
		} else
			prop->prop_peerspi = msg->msg_prop->prop_peerspi;

		/* Set rekeying flags on Child SAs */
		if (rekeying) {
			if ((csa = childsa_lookup(sa, rekey->spi,
			    rekey->spi_protoid)) == NULL) {
				log_debug("%s: CHILD SA %s wasn't found",
				    __func__, print_spi(rekey->spi,
					rekey->spi_size));
				goto fail;
			}
			if (!csa->csa_loaded || !csa->csa_peersa ||
			    !csa->csa_peersa->csa_loaded) {
				log_debug("%s: SA is not loaded or no peer SA",
				    __func__);
				goto fail;
			}
			csa->csa_rekey = 1;
			csa->csa_peersa->csa_rekey = 1;
		}

		/* Update initiator's nonce */
		if (!ibuf_length(msg->msg_nonce)) {
			log_debug("%s: initiator didn't send nonce", __func__);
			goto fail;
		}
		ibuf_release(kex->kex_inonce);
		kex->kex_inonce = ibuf_dup(msg->msg_nonce);

		/* Generate new responder's nonce */
		if ((nonce = ibuf_random(IKED_NONCE_SIZE)) == NULL)
			goto fail;

		/* Update responder's nonce */
		ibuf_release(kex->kex_rnonce);
		kex->kex_rnonce = nonce;

		if (ikev2_childsa_negotiate(env, sa, kex, &proposals, 0, pfs)) {
			log_debug("%s: failed to get CHILD SAs", __func__);
			goto fail;
		}
	}

	if ((e = ibuf_static()) == NULL)
		goto done;

	/* compression (unless IKE rekeying) */
	if (!nsa && sa->sa_ipcomp &&
	    (len = ikev2_add_ipcompnotify(env, e, &pld, 0, sa)) == -1)
		goto done;

	if (pld) {
		firstpayload = IKEV2_PAYLOAD_NOTIFY;
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_SA) == -1)
			goto done;
	} else
		firstpayload = IKEV2_PAYLOAD_SA;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;

	if ((len = ikev2_add_proposals(env, nsa ? nsa : sa, e,
		nsa ? &nsa->sa_proposals : &proposals,
		protoid, 0, nsa ? 1 : 0)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if (ikev2_add_buf(e, nonce) == -1)
		goto done;
	len = ibuf_size(nonce);

	if (protoid == IKEV2_SAPROTO_IKE || pfs) {

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_KE) == -1)
			goto done;

		/* KE payload */
		if ((pld = ikev2_add_payload(e)) == NULL)
			goto done;
		if ((ke = ibuf_advance(e, sizeof(*ke))) == NULL)
			goto done;
		if (kex->kex_dhgroup == NULL) {
			log_debug("%s: invalid dh", __func__);
			goto done;
		}
		ke->kex_dhgroup = htobe16(kex->kex_dhgroup->id);
		if (ikev2_add_buf(e, kex->kex_dhrexchange) == -1)
			goto done;
		len = sizeof(*ke) + dh_getlen(kex->kex_dhgroup);
	}

	if (protoid != IKEV2_SAPROTO_IKE)
		if ((len = ikev2_add_ts(e, &pld, len, sa, initiator)) == -1)
			goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if ((ret = ikev2_msg_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_CREATE_CHILD_SA, firstpayload, 1)) == -1)
		goto done;

	if (protoid == IKEV2_SAPROTO_IKE)
		ret = ikev2_ikesa_enable(env, sa, nsa);
	else
		ret = ikev2_childsa_enable(env, sa);

 done:
	if (ret && protoid != IKEV2_SAPROTO_IKE)
		ikev2_childsa_delete(env, sa, 0, 0, NULL, 1);
	ibuf_release(e);
	config_free_proposals(&proposals, 0);
	config_free_kex(kextmp);
	return (ret);

 fail:
	config_free_proposals(&proposals, 0);
	config_free_kex(kextmp);
	return (-1);
}

void
ikev2_ike_sa_timeout(struct iked *env, void *arg)
{
	struct iked_sa			*sa = arg;

	log_debug("%s: closing SA", __func__);
	sa_free(env, sa);
}

void
ikev2_ike_sa_alive(struct iked *env, void *arg)
{
	struct iked_sa			*sa = arg;
	struct iked_childsa		*csa = NULL;
	struct timeval			 tv;
	u_int64_t			 last_used, diff;
	int				 foundin = 0, foundout = 0;

	/* check for incoming traffic on any child SA */
	TAILQ_FOREACH(csa, &sa->sa_childsas, csa_entry) {
		if (!csa->csa_loaded ||
		    csa->csa_saproto == IKEV2_SAPROTO_IPCOMP)
			continue;
		if (pfkey_sa_last_used(env->sc_pfkey, csa, &last_used) != 0)
			continue;
		gettimeofday(&tv, NULL);
		diff = (u_int32_t)(tv.tv_sec - last_used);
		log_debug("%s: %s CHILD SA spi %s last used %llu second(s) ago",
		    __func__,
		    csa->csa_dir == IPSP_DIRECTION_IN ? "incoming" : "outgoing",
		    print_spi(csa->csa_spi.spi, csa->csa_spi.spi_size), diff);
		if (diff < IKED_IKE_SA_ALIVE_TIMEOUT) {
			if (csa->csa_dir == IPSP_DIRECTION_IN) {
				foundin = 1;
				break;
			} else {
				foundout = 1;
			}
		}
	}

	/* send probe if any outging SA has been used, but no incoming SA */
	if (!foundin && foundout) {
		log_debug("%s: sending alive check", __func__);
		ikev2_send_ike_e(env, sa, NULL, IKEV2_PAYLOAD_NONE,
		    IKEV2_EXCHANGE_INFORMATIONAL, 0);
		sa->sa_stateflags |= IKED_REQ_INF;
	}

	/* re-register */
	timer_add(env, &sa->sa_timer, IKED_IKE_SA_ALIVE_TIMEOUT);
}

int
ikev2_send_informational(struct iked *env, struct iked_message *msg)
{
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ikev2_notify		*n;
	struct iked_sa			*sa = msg->msg_sa, sah;
	struct ibuf			*buf, *e = NULL;
	int				 ret = -1;

	if (msg->msg_error == 0)
		return (0);

	if ((buf = ikev2_msg_init(env, &resp,
	    &msg->msg_peer, msg->msg_peerlen,
	    &msg->msg_local, msg->msg_locallen, 0)) == NULL)
		goto done;

	/* New encrypted message buffer */
	if ((e = ibuf_static()) == NULL)
		goto done;

	/* NOTIFY payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;

	if ((n = ibuf_advance(e, sizeof(*n))) == NULL)
		goto done;
	n->n_protoid = IKEV2_SAPROTO_IKE;	/* XXX ESP etc. */
	n->n_spisize = 0;
	n->n_type = htobe16(msg->msg_error);

	switch (msg->msg_error) {
	case IKEV2_N_INVALID_IKE_SPI:
	case IKEV2_N_NO_PROPOSAL_CHOSEN:
		break;
	default:
		log_debug("%s: unsupported notification %s", __func__,
		    print_map(msg->msg_error, ikev2_n_map));
		goto done;
	}

	if (ikev2_next_payload(pld, sizeof(*n), IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if (sa != NULL && msg->msg_e) {
		resp.msg_msgid = ikev2_msg_id(env, sa);

		/* IKE header */
		if ((hdr = ikev2_add_header(buf, sa, resp.msg_msgid,
		    IKEV2_PAYLOAD_SK, IKEV2_EXCHANGE_INFORMATIONAL,
		    0)) == NULL)
			goto done;

		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;

		/* Encrypt message and add as an E payload */
		if ((e = ikev2_msg_encrypt(env, sa, e)) == NULL) {
			log_debug("%s: encryption failed", __func__);
			goto done;
		}
		if (ibuf_cat(buf, e) != 0)
			goto done;
		if (ikev2_next_payload(pld, ibuf_size(e),
		    IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
			goto done;

		/* Add integrity checksum (HMAC) */
		if (ikev2_msg_integr(env, sa, buf) != 0) {
			log_debug("%s: integrity checksum failed", __func__);
			goto done;
		}
	} else {
		if ((hdr = ibuf_seek(msg->msg_data, 0, sizeof(*hdr))) == NULL)
			goto done;

		bzero(&sah, sizeof(sah));
		sah.sa_hdr.sh_rspi = betoh64(hdr->ike_rspi);
		sah.sa_hdr.sh_ispi = betoh64(hdr->ike_ispi);
		sah.sa_hdr.sh_initiator =
		    hdr->ike_flags & IKEV2_FLAG_INITIATOR ? 0 : 1;

		resp.msg_msgid = ikev2_msg_id(env, &sah);

		/* IKE header */
		if ((hdr = ikev2_add_header(buf, &sah, resp.msg_msgid,
		    IKEV2_PAYLOAD_NOTIFY, IKEV2_EXCHANGE_INFORMATIONAL,
		    0)) == NULL)
			goto done;
		if (ibuf_cat(buf, e) != 0)
			goto done;
		if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
			goto done;
	}

	resp.msg_data = buf;
	resp.msg_fd = msg->msg_fd;
	TAILQ_INIT(&resp.msg_proposals);

	ret = ikev2_msg_send(env, &resp);

 done:
	ibuf_release(e);
	ikev2_msg_cleanup(env, &resp);

	return (ret);
}

ssize_t
ikev2_psk(struct iked_sa *sa, u_int8_t *data, size_t length,
    u_int8_t **pskptr)
{
	u_int8_t	*psk;
	size_t		 psklen = -1;

	if (hash_setkey(sa->sa_prf, data, length) == NULL)
		return (-1);

	if ((psk = calloc(1, hash_keylength(sa->sa_prf))) == NULL)
		return (-1);

	hash_init(sa->sa_prf);
	hash_update(sa->sa_prf, IKEV2_KEYPAD, strlen(IKEV2_KEYPAD));
	hash_final(sa->sa_prf, psk, &psklen);

	*pskptr = psk;
	return (psklen);
}

int
ikev2_match_proposals(struct iked_proposal *local, struct iked_proposal *peer,
    struct iked_transform **xforms)
{
	struct iked_transform	*tpeer, *tlocal;
	u_int			 i, j, type, score;
	u_int8_t		 protoid = peer->prop_protoid;

	for (i = 0; i < peer->prop_nxforms; i++) {
		tpeer = peer->prop_xforms + i;
		for (j = 0; j < local->prop_nxforms; j++) {
			tlocal = local->prop_xforms + j;
			if (tpeer->xform_type != tlocal->xform_type ||
			    tpeer->xform_id != tlocal->xform_id ||
			    tpeer->xform_length != tlocal->xform_length)
				continue;
			if (tpeer->xform_type > IKEV2_XFORMTYPE_MAX)
				continue;
			type = tpeer->xform_type;

			if (xforms[type] == NULL || tlocal->xform_score <
			    xforms[type]->xform_score) {
				xforms[type] = tlocal;
			} else
				continue;

			print_debug("%s: xform %d <-> %d (%d): %s %s "
			    "(keylength %d <-> %d)", __func__,
			    peer->prop_id, local->prop_id, tlocal->xform_score,
			    print_map(type, ikev2_xformtype_map),
			    print_map(tpeer->xform_id, tpeer->xform_map),
			    tpeer->xform_keylength, tlocal->xform_keylength);
			if (tpeer->xform_length)
				print_debug(" %d", tpeer->xform_length);
			print_debug("\n");
		}
	}

	for (i = score = 0; i < IKEV2_XFORMTYPE_MAX; i++) {
		if (protoid == IKEV2_SAPROTO_IKE && xforms[i] == NULL &&
		    (i == IKEV2_XFORMTYPE_ENCR || i == IKEV2_XFORMTYPE_PRF ||
		     i == IKEV2_XFORMTYPE_INTEGR || i == IKEV2_XFORMTYPE_DH)) {
			score = 0;
			break;
		} else if (protoid == IKEV2_SAPROTO_AH && xforms[i] == NULL &&
		    (i == IKEV2_XFORMTYPE_INTEGR || i == IKEV2_XFORMTYPE_ESN)) {
			score = 0;
			break;
		} else if (protoid == IKEV2_SAPROTO_ESP && xforms[i] == NULL &&
		    (i == IKEV2_XFORMTYPE_ENCR || i == IKEV2_XFORMTYPE_ESN)) {
			score = 0;
			break;
		} else if (xforms[i] == NULL)
			continue;

		score += xforms[i]->xform_score;
	}

	return (score);
}

int
ikev2_sa_negotiate(struct iked_proposals *result, struct iked_proposals *local,
    struct iked_proposals *peer)
{
	struct iked_proposal	*ppeer = NULL, *plocal, *prop, vpeer, vlocal;
	struct iked_transform	 chosen[IKEV2_XFORMTYPE_MAX];
	struct iked_transform	*valid[IKEV2_XFORMTYPE_MAX];
	struct iked_transform	*match[IKEV2_XFORMTYPE_MAX];
	u_int			 i, score, chosen_score = 0;
	u_int8_t		 protoid = 0;

	bzero(valid, sizeof(valid));
	bzero(&vlocal, sizeof(vlocal));
	bzero(&vpeer, sizeof(vpeer));

	if (TAILQ_EMPTY(peer)) {
		log_debug("%s: peer did not send %s proposals", __func__,
		    print_map(protoid, ikev2_saproto_map));
		return (-1);
	}

	TAILQ_FOREACH(plocal, local, prop_entry) {
		TAILQ_FOREACH(ppeer, peer, prop_entry) {
			if (ppeer->prop_protoid != plocal->prop_protoid)
				continue;
			bzero(match, sizeof(match));
			score = ikev2_match_proposals(plocal, ppeer, match);
			log_debug("%s: score %d", __func__, score);
			if (score && (!chosen_score || score < chosen_score)) {
				chosen_score = score;
				for (i = 0; i < IKEV2_XFORMTYPE_MAX; i++) {
					if ((valid[i] = match[i]))
						memcpy(&chosen[i], match[i],
						    sizeof(chosen[0]));
				}
				memcpy(&vpeer, ppeer, sizeof(vpeer));
				memcpy(&vlocal, plocal, sizeof(vlocal));
			}
		}
		if (chosen_score != 0)
			break;
	}

	if (chosen_score == 0)
		return (-1);
	else if (result == NULL)
		return (0);

	(void)config_free_proposals(result, vpeer.prop_protoid);
	prop = config_add_proposal(result, vpeer.prop_id, vpeer.prop_protoid);

	if (vpeer.prop_localspi.spi_size) {
		prop->prop_localspi.spi_size = vpeer.prop_localspi.spi_size;
		prop->prop_peerspi = vpeer.prop_peerspi;
	}
	if (vlocal.prop_localspi.spi_size) {
		prop->prop_localspi.spi_size = vlocal.prop_localspi.spi_size;
		prop->prop_localspi.spi = vlocal.prop_localspi.spi;
	}

	for (i = 0; i < IKEV2_XFORMTYPE_MAX; i++) {
		if (valid[i] == NULL)
			continue;
		print_debug("%s: score %d: %s %s", __func__,
		    chosen[i].xform_score, print_map(i, ikev2_xformtype_map),
		    print_map(chosen[i].xform_id, chosen[i].xform_map));
		if (chosen[i].xform_length)
			print_debug(" %d", chosen[i].xform_length);
		print_debug("\n");

		if (config_add_transform(prop, chosen[i].xform_type,
		    chosen[i].xform_id, chosen[i].xform_length,
		    chosen[i].xform_keylength) == NULL)
			break;
	}

	return (0);
}

int
ikev2_sa_initiator_dh(struct iked_sa *sa, struct iked_message *msg, u_int proto)
{
	struct iked_policy	*pol = sa->sa_policy;
	struct iked_transform	*xform;

	if (sa->sa_dhgroup == NULL) {
		if ((xform = config_findtransform(&pol->pol_proposals,
		    IKEV2_XFORMTYPE_DH, proto)) == NULL) {
			log_debug("%s: did not find dh transform", __func__);
			return (-1);
		}
		if ((sa->sa_dhgroup =
		    group_get(xform->xform_id)) == NULL) {
			log_debug("%s: invalid dh %d", __func__,
			    xform->xform_id);
			return (-1);
		}
	}

	if (!ibuf_length(sa->sa_dhiexchange)) {
		if ((sa->sa_dhiexchange = ibuf_new(NULL,
		    dh_getlen(sa->sa_dhgroup))) == NULL) {
			log_debug("%s: failed to alloc dh exchange", __func__);
			return (-1);
		}
		if (dh_create_exchange(sa->sa_dhgroup,
		    sa->sa_dhiexchange->buf) == -1) {
			log_debug("%s: failed to get dh exchange", __func__);
			return (-1);
		}
	}

	/* Initial message */
	if (msg == NULL)
		return (0);

	if (!ibuf_length(sa->sa_dhrexchange)) {
		if (!ibuf_length(msg->msg_ke)) {
			log_debug("%s: invalid peer dh exchange", __func__);
			return (-1);
		}
		if ((ssize_t)ibuf_length(msg->msg_ke) !=
		    dh_getlen(sa->sa_dhgroup)) {
			log_debug("%s: invalid dh length, size %d", __func__,
			    dh_getlen(sa->sa_dhgroup) * 8);
			return (-1);
		}
		if ((sa->sa_dhrexchange = ibuf_dup(msg->msg_ke)) == NULL) {
			log_debug("%s: failed to copy dh exchange", __func__);
			return (-1);
		}
	}

	/* Set a pointer to the peer exchange */
	sa->sa_dhpeer = sa->sa_dhrexchange;
	return (0);
}

int
ikev2_sa_initiator(struct iked *env, struct iked_sa *sa,
    struct iked_sa *osa, struct iked_message *msg)
{
	struct iked_transform	*xform;

	if (ikev2_sa_initiator_dh(sa, msg, 0) < 0)
		return (-1);

	if (!ibuf_length(sa->sa_inonce)) {
		if ((sa->sa_inonce = ibuf_random(IKED_NONCE_SIZE)) == NULL) {
			log_debug("%s: failed to get local nonce", __func__);
			return (-1);
		}
	}

	/* Initial message */
	if (msg == NULL)
		return (0);

	if (!ibuf_length(sa->sa_rnonce)) {
		if (!ibuf_length(msg->msg_nonce)) {
			log_debug("%s: invalid peer nonce", __func__);
			return (-1);
		}
		if ((sa->sa_rnonce = ibuf_dup(msg->msg_nonce)) == NULL) {
			log_debug("%s: failed to get peer nonce", __func__);
			return (-1);
		}
	}

	/* XXX we need a better way to get this */
	if (ikev2_sa_negotiate(&sa->sa_proposals,
	    &msg->msg_policy->pol_proposals, &msg->msg_proposals) != 0) {
		log_debug("%s: no proposal chosen", __func__);
		msg->msg_error = IKEV2_N_NO_PROPOSAL_CHOSEN;
		return (-1);
	} else if (sa_stateok(sa, IKEV2_STATE_SA_INIT))
		sa_stateflags(sa, IKED_REQ_SA);

	if (sa->sa_encr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_ENCR, 0)) == NULL) {
			log_debug("%s: did not find encr transform", __func__);
			return (-1);
		}
		if ((sa->sa_encr = cipher_new(xform->xform_type,
		    xform->xform_id, xform->xform_length)) == NULL) {
			log_debug("%s: failed to get encr", __func__);
			return (-1);
		}
	}

	if (sa->sa_prf == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_PRF, 0)) == NULL) {
			log_debug("%s: did not find prf transform", __func__);
			return (-1);
		}
		if ((sa->sa_prf =
		    hash_new(xform->xform_type, xform->xform_id)) == NULL) {
			log_debug("%s: failed to get prf", __func__);
			return (-1);
		}
	}

	if (sa->sa_integr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_INTEGR, 0)) == NULL) {
			log_debug("%s: did not find integr transform",
			    __func__);
			return (-1);
		}
		if ((sa->sa_integr =
		    hash_new(xform->xform_type, xform->xform_id)) == NULL) {
			log_debug("%s: failed to get integr", __func__);
			return (-1);
		}
	}

	ibuf_release(sa->sa_2ndmsg);
	if ((sa->sa_2ndmsg = ibuf_dup(msg->msg_data)) == NULL) {
		log_debug("%s: failed to copy 2nd message", __func__);
		return (-1);
	}

	return (ikev2_sa_keys(env, sa, osa ? osa->sa_key_d : NULL));
}

int
ikev2_sa_responder_dh(struct iked_kex *kex, struct iked_proposals *proposals,
    struct iked_message *msg, u_int proto)
{
	struct iked_transform	*xform;

	if (kex->kex_dhgroup == NULL) {
		if ((xform = config_findtransform(proposals,
		    IKEV2_XFORMTYPE_DH, proto)) == NULL) {
			log_debug("%s: did not find dh transform", __func__);
			return (-1);
		}
		if ((kex->kex_dhgroup =
		    group_get(xform->xform_id)) == NULL) {
			log_debug("%s: invalid dh %d", __func__,
			    xform->xform_id);
			return (-1);
		}
	}

	if (!ibuf_length(kex->kex_dhrexchange)) {
		if ((kex->kex_dhrexchange = ibuf_new(NULL,
		    dh_getlen(kex->kex_dhgroup))) == NULL) {
			log_debug("%s: failed to alloc dh exchange", __func__);
			return (-1);
		}
		if (dh_create_exchange(kex->kex_dhgroup,
		    kex->kex_dhrexchange->buf) == -1) {
			log_debug("%s: failed to get dh exchange", __func__);
			return (-1);
		}
	}

	if (!ibuf_length(kex->kex_dhiexchange)) {
		if ((kex->kex_dhiexchange = ibuf_dup(msg->msg_ke)) == NULL ||
		    ((ssize_t)ibuf_length(kex->kex_dhiexchange) !=
		    dh_getlen(kex->kex_dhgroup))) {
			/* XXX send notification to peer */
			log_debug("%s: invalid dh, size %d", __func__,
			    dh_getlen(kex->kex_dhgroup) * 8);
			return (-1);
		}
	}

	/* Set a pointer to the peer exchange */
	kex->kex_dhpeer = kex->kex_dhiexchange;
	return (0);
}
int
ikev2_sa_responder(struct iked *env, struct iked_sa *sa, struct iked_sa *osa,
    struct iked_message *msg)
{
	struct iked_transform	*xform;

	sa_state(env, sa, IKEV2_STATE_SA_INIT);

	ibuf_release(sa->sa_1stmsg);
	if ((sa->sa_1stmsg = ibuf_dup(msg->msg_data)) == NULL) {
		log_debug("%s: failed to copy 1st message", __func__);
		return (-1);
	}

	if (!ibuf_length(sa->sa_rnonce) &&
	    (sa->sa_rnonce = ibuf_random(IKED_NONCE_SIZE)) == NULL) {
		log_debug("%s: failed to get local nonce", __func__);
		return (-1);
	}

	if (!ibuf_length(sa->sa_inonce) &&
	    ((ibuf_length(msg->msg_nonce) < IKED_NONCE_MIN) ||
	    (sa->sa_inonce = ibuf_dup(msg->msg_nonce)) == NULL)) {
		log_debug("%s: failed to get peer nonce", __func__);
		return (-1);
	}

	/* XXX we need a better way to get this */
	if (ikev2_sa_negotiate(&sa->sa_proposals,
	    &msg->msg_policy->pol_proposals, &msg->msg_proposals) != 0) {
		log_debug("%s: no proposal chosen", __func__);
		msg->msg_error = IKEV2_N_NO_PROPOSAL_CHOSEN;
		return (-1);
	} else if (sa_stateok(sa, IKEV2_STATE_SA_INIT))
		sa_stateflags(sa, IKED_REQ_SA);

	if (sa->sa_encr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_ENCR, 0)) == NULL) {
			log_debug("%s: did not find encr transform", __func__);
			return (-1);
		}
		if ((sa->sa_encr = cipher_new(xform->xform_type,
		    xform->xform_id, xform->xform_length)) == NULL) {
			log_debug("%s: failed to get encr", __func__);
			return (-1);
		}
	}

	if (sa->sa_prf == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_PRF, 0)) == NULL) {
			log_debug("%s: did not find prf transform", __func__);
			return (-1);
		}
		if ((sa->sa_prf =
		    hash_new(xform->xform_type, xform->xform_id)) == NULL) {
			log_debug("%s: failed to get prf", __func__);
			return (-1);
		}
	}

	if (sa->sa_integr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_INTEGR, 0)) == NULL) {
			log_debug("%s: did not find integr transform",
			    __func__);
			return (-1);
		}
		if ((sa->sa_integr =
		    hash_new(xform->xform_type, xform->xform_id)) == NULL) {
			log_debug("%s: failed to get integr", __func__);
			return (-1);
		}
	}

	if (ikev2_sa_responder_dh(&sa->sa_kex, &sa->sa_proposals, msg, 0) < 0)
		return (-1);

	return (ikev2_sa_keys(env, sa, osa ? osa->sa_key_d : NULL));
}

int
ikev2_sa_keys(struct iked *env, struct iked_sa *sa, struct ibuf *key)
{
	struct iked_hash	*prf, *integr;
	struct iked_cipher	*encr;
	struct group		*group;
	struct ibuf		*ninr, *dhsecret, *skeyseed, *s, *t;
	size_t			 nonceminlen, ilen, rlen, tmplen;
	u_int64_t		 ispi, rspi;
	int			 ret = -1;

	ninr = dhsecret = skeyseed = s = t = NULL;

	if ((encr = sa->sa_encr) == NULL ||
	    (prf = sa->sa_prf) == NULL ||
	    (integr = sa->sa_integr) == NULL ||
	    (group = sa->sa_dhgroup) == NULL) {
		log_debug("%s: failed to get key input data", __func__);
		return (-1);
	}

	if (prf->hash_fixedkey)
		nonceminlen = prf->hash_fixedkey;
	else
		nonceminlen = IKED_NONCE_MIN;

	/* Nonces need a minimal size and should have an even length */
	if (ibuf_length(sa->sa_inonce) < nonceminlen ||
	    (ibuf_length(sa->sa_inonce) % 2) != 0 ||
	    ibuf_length(sa->sa_rnonce) < nonceminlen ||
	    (ibuf_length(sa->sa_rnonce) % 2) != 0) {
		log_debug("%s: invalid nonces", __func__);
		return (-1);
	}

	if (prf->hash_fixedkey) {
		/* Half of the key bits must come from Ni, and half from Nr */
		ilen = prf->hash_fixedkey / 2;
		rlen = prf->hash_fixedkey / 2;
	} else {
		/* Most PRF functions accept a variable-length key */
		ilen = ibuf_length(sa->sa_inonce);
		rlen = ibuf_length(sa->sa_rnonce);
	}

	/*
	 *  Depending on whether we're generating new keying material
	 *  or rekeying existing SA the algorithm is different. If the
	 *  "key" argument is not specified a concatenation of nonces
	 *  (Ni | Nr) is used as a PRF key, otherwise a "key" buffer
	 *  is used and PRF is performed on the concatenation of DH
	 *  exchange result and nonces (g^ir | Ni | Nr).  See sections
	 *  2.14 and 2.18 of RFC5996 for more information.
	 */

	/*
	 *  Generate g^ir
	 */
	if ((dhsecret = ibuf_new(NULL, dh_getlen(group))) == NULL) {
		log_debug("%s: failed to alloc dh secret", __func__);
		goto done;
	}
	if (dh_create_shared(group, dhsecret->buf,
	    sa->sa_dhpeer->buf) == -1) {
		log_debug("%s: failed to get dh secret"
		    " group %d len %d secret %zu exchange %zu", __func__,
		    group->id, dh_getlen(group), ibuf_length(dhsecret),
		    ibuf_length(sa->sa_dhpeer));
		goto done;
	}

	if (!key) {
		/*
		 * Set PRF key to generate SKEEYSEED = prf(Ni | Nr, g^ir)
		 */
		if ((ninr = ibuf_new(sa->sa_inonce->buf, ilen)) == NULL ||
		    ibuf_add(ninr, sa->sa_rnonce->buf, rlen) != 0) {
			log_debug("%s: failed to get nonce key buffer",
			    __func__);
			goto done;
		}
		key = ninr;
	} else {
		/*
		 * Set PRF key to generate SKEEYSEED = prf(key, g^ir | Ni | Nr)
		 */
		if (ibuf_add(dhsecret, sa->sa_inonce->buf, ilen) != 0 ||
		    ibuf_add(dhsecret, sa->sa_rnonce->buf, rlen) != 0) {
			log_debug("%s: failed to get nonce key buffer",
			    __func__);
			goto done;
		}
	}

	if ((hash_setkey(prf, key->buf, ibuf_length(key))) == NULL) {
		log_debug("%s: failed to set prf key", __func__);
		goto done;
	}

	if ((skeyseed = ibuf_new(NULL, hash_length(prf))) == NULL) {
		log_debug("%s: failed to get SKEYSEED buffer", __func__);
		goto done;
	}

	tmplen = 0;
	hash_init(prf);
	hash_update(prf, dhsecret->buf, ibuf_length(dhsecret));
	hash_final(prf, skeyseed->buf, &tmplen);

	log_debug("%s: SKEYSEED with %zu bytes", __func__, tmplen);
	print_hex(skeyseed->buf, 0, tmplen);

	if (ibuf_setsize(skeyseed, tmplen) == -1) {
		log_debug("%s: failed to set keymaterial length", __func__);
		goto done;
	}

	/*
	 * Now generate the key material
	 *
	 * S = Ni | Nr | SPIi | SPIr
	 */

	/* S = Ni | Nr | SPIi | SPIr */
	ilen = ibuf_length(sa->sa_inonce);
	rlen = ibuf_length(sa->sa_rnonce);
	ispi = htobe64(sa->sa_hdr.sh_ispi);
	rspi = htobe64(sa->sa_hdr.sh_rspi);

	if ((s = ibuf_new(sa->sa_inonce->buf, ilen)) == NULL ||
	    ibuf_add(s, sa->sa_rnonce->buf, rlen) != 0 ||
	    ibuf_add(s, &ispi, sizeof(ispi)) != 0 ||
	    ibuf_add(s, &rspi, sizeof(rspi)) != 0) {
		log_debug("%s: failed to set S buffer", __func__);
		goto done;
	}

	log_debug("%s: S with %zu bytes", __func__, ibuf_length(s));
	print_hex(s->buf, 0, ibuf_length(s));

	/*
	 * Get the size of the key material we need and the number
	 * of rounds we need to run the prf+ function.
	 */
	ilen = hash_length(prf) +	/* SK_d */
	    hash_keylength(integr) +	/* SK_ai */
	    hash_keylength(integr) +	/* SK_ar */
	    cipher_keylength(encr) +	/* SK_ei */
	    cipher_keylength(encr) +	/* SK_er */
	    hash_keylength(prf) +	/* SK_pi */
	    hash_keylength(prf);	/* SK_pr */

	if ((t = ikev2_prfplus(prf, skeyseed, s, ilen)) == NULL) {
		log_debug("%s: failed to get IKE SA key material", __func__);
		goto done;
	}

	/* ibuf_get() returns a new buffer from the next read offset */
	if ((sa->sa_key_d = ibuf_get(t, hash_length(prf))) == NULL ||
	    (sa->sa_key_iauth = ibuf_get(t, hash_keylength(integr))) == NULL ||
	    (sa->sa_key_rauth = ibuf_get(t, hash_keylength(integr))) == NULL ||
	    (sa->sa_key_iencr = ibuf_get(t, cipher_keylength(encr))) == NULL ||
	    (sa->sa_key_rencr = ibuf_get(t, cipher_keylength(encr))) == NULL ||
	    (sa->sa_key_iprf = ibuf_get(t, hash_length(prf))) == NULL ||
	    (sa->sa_key_rprf = ibuf_get(t, hash_length(prf))) == NULL) {
		log_debug("%s: failed to get SA keys", __func__);
		goto done;
	}

	log_debug("%s: SK_d with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_d));
	print_hex(sa->sa_key_d->buf, 0, ibuf_length(sa->sa_key_d));
	log_debug("%s: SK_ai with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_iauth));
	print_hex(sa->sa_key_iauth->buf, 0, ibuf_length(sa->sa_key_iauth));
	log_debug("%s: SK_ar with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_rauth));
	print_hex(sa->sa_key_rauth->buf, 0, ibuf_length(sa->sa_key_rauth));
	log_debug("%s: SK_ei with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_iencr));
	print_hex(sa->sa_key_iencr->buf, 0, ibuf_length(sa->sa_key_iencr));
	log_debug("%s: SK_er with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_rencr));
	print_hex(sa->sa_key_rencr->buf, 0, ibuf_length(sa->sa_key_rencr));
	log_debug("%s: SK_pi with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_iprf));
	print_hex(sa->sa_key_iprf->buf, 0, ibuf_length(sa->sa_key_iprf));
	log_debug("%s: SK_pr with %zu bytes", __func__,
	    ibuf_length(sa->sa_key_rprf));
	print_hex(sa->sa_key_rprf->buf, 0, ibuf_length(sa->sa_key_rprf));

	ret = 0;

 done:
	ibuf_release(ninr);
	ibuf_release(dhsecret);
	ibuf_release(skeyseed);
	ibuf_release(s);
	ibuf_release(t);

	return (ret);
}

void
ikev2_sa_cleanup_dh(struct iked_sa *sa)
{
	ibuf_release(sa->sa_dhiexchange);
	ibuf_release(sa->sa_dhrexchange);
	group_free(sa->sa_dhgroup);
	sa->sa_dhiexchange = NULL;
	sa->sa_dhrexchange = NULL;
	sa->sa_dhgroup = NULL;
}

struct ibuf *
ikev2_prfplus(struct iked_hash *prf, struct ibuf *key, struct ibuf *seed,
    size_t keymatlen)
{
	struct ibuf	*t = NULL, *t1 = NULL, *t2 = NULL;
	size_t		 rlen, i, hashlen = 0;
	u_int8_t	 pad = 0;

	/*
	 * prf+ (K, S) = T1 | T2 | T3 | T4 | ...
	 *
	 * T1 = prf (K, S | 0x01)
	 * T2 = prf (K, T1 | S | 0x02)
	 * T3 = prf (K, T2 | S | 0x03)
	 * T4 = prf (K, T3 | S | 0x04)
	 */

	if ((hash_setkey(prf, ibuf_data(key), ibuf_size(key))) == NULL) {
		log_debug("%s: failed to set prf+ key", __func__);
		goto fail;
	}

	if ((t = ibuf_new(NULL, 0)) == NULL) {
		log_debug("%s: failed to get T buffer", __func__);
		goto fail;
	}

	rlen = roundup(keymatlen, hash_length(prf)) / hash_length(prf);
	if (rlen > 255)
		fatalx("ikev2_prfplus: key material too large");

	for (i = 0; i < rlen; i++) {
		if (t1 != NULL) {
			t2 = ibuf_new(t1->buf, ibuf_length(t1));
			ibuf_release(t1);
		} else
			t2 = ibuf_new(NULL, 0);
		t1 = ibuf_new(NULL, hash_length(prf));

		ibuf_add(t2, seed->buf, ibuf_length(seed));
		pad = i + 1;
		ibuf_add(t2, &pad, 1);

		hash_init(prf);
		hash_update(prf, t2->buf, ibuf_length(t2));
		hash_final(prf, t1->buf, &hashlen);

		if (hashlen != hash_length(prf))
			fatalx("ikev2_prfplus: hash length mismatch");

		ibuf_release(t2);
		ibuf_add(t, t1->buf, ibuf_length(t1));

		log_debug("%s: T%d with %zu bytes", __func__,
		    pad, ibuf_length(t1));
		print_hex(t1->buf, 0, ibuf_length(t1));
	}

	log_debug("%s: Tn with %zu bytes", __func__, ibuf_length(t));
	print_hex(t->buf, 0, ibuf_length(t));

	ibuf_release(t1);

	return (t);

 fail:
	ibuf_release(t1);
	ibuf_release(t);

	return (NULL);
}

int
ikev2_sa_tag(struct iked_sa *sa, struct iked_id *id)
{
	char	*format, *domain = NULL, *idrepl = NULL;
	char	 idstr[IKED_ID_SIZE];
	int	 ret = -1;
	size_t	 len;

	if (sa->sa_tag != NULL)
		free(sa->sa_tag);
	sa->sa_tag = NULL;
	format = sa->sa_policy->pol_tag;

	len = IKED_TAG_SIZE;
	if ((sa->sa_tag = calloc(1, len)) == NULL) {
		log_debug("%s: calloc", __func__);
		goto fail;
	}
	if (strlcpy(sa->sa_tag, format, len) >= len) {
		log_debug("%s: tag too long", __func__);
		goto fail;
	}

	if (ikev2_print_id(id, idstr, sizeof(idstr)) == -1) {
		log_debug("%s: invalid id", __func__);
		goto fail;
	}

	/* ASN.1 DER IDs are too long, use the CN part instead */
	if ((id->id_type == IKEV2_ID_ASN1_DN) &&
	    (idrepl = strstr(idstr, "CN=")) != NULL) {
		domain = strstr(idrepl, "emailAddress=");
		idrepl[strcspn(idrepl, "/")] = '\0';
	} else
		idrepl = idstr;

	if (strstr(format, "$id") != NULL) {
		if (expand_string(sa->sa_tag, len, "$id", idrepl) != 0) {
			log_debug("%s: failed to expand tag", __func__);
			goto fail;
		}
	}

	if (strstr(format, "$name") != NULL) {
		if (expand_string(sa->sa_tag, len, "$name",
		    sa->sa_policy->pol_name) != 0) {
			log_debug("%s: failed to expand tag", __func__);
			goto fail;
		}
	}

	if (strstr(format, "$domain") != NULL) {
		if (id->id_type == IKEV2_ID_FQDN)
			domain = strchr(idrepl, '.');
		else if (id->id_type == IKEV2_ID_UFQDN)
			domain = strchr(idrepl, '@');
		else if (*idstr == '/' && domain != NULL)
			domain = strchr(domain, '@');
		else
			domain = NULL;
		if (domain == NULL || strlen(domain) < 2) {
			log_debug("%s: no valid domain in ID %s",
			    __func__, idstr);
			goto fail;
		}
		domain++;
		if (expand_string(sa->sa_tag, len, "$domain", domain) != 0) {
			log_debug("%s: failed to expand tag", __func__);
			goto fail;
		}
	}

	log_debug("%s: %s (%zu)", __func__, sa->sa_tag, strlen(sa->sa_tag));

	ret = 0;
 fail:
	if (ret != 0) {
		free(sa->sa_tag);
		sa->sa_tag = NULL;
	}

	return (ret);
}

int
ikev2_childsa_negotiate(struct iked *env, struct iked_sa *sa,
    struct iked_kex *kex, struct iked_proposals *proposals, int initiator,
    int pfs)
{
	struct iked_proposal	*prop;
	struct iked_transform	*xform, *encrxf = NULL, *integrxf = NULL;
	struct iked_childsa	*csa, *csb;
	struct iked_flow	*flow, *saflow, *flowa, *flowb;
	struct ibuf		*keymat = NULL, *seed = NULL, *dhsecret = NULL;
	struct group		*group;
	u_int32_t		 spi = 0;
	u_int			 i;
	size_t			 ilen = 0;
	int			 esn, skip, ret = -1;

	if (!sa_stateok(sa, IKEV2_STATE_VALID))
		return (-1);

	if (ikev2_sa_tag(sa, IKESA_DSTID(sa)) == -1)
		return (-1);

	/* We need to determine the key material length first */
	TAILQ_FOREACH(prop, proposals, prop_entry) {
		if (prop->prop_protoid == IKEV2_SAPROTO_IKE)
			continue;
		log_debug("%s: proposal %d", __func__, prop->prop_id);
		for (i = 0; i < prop->prop_nxforms; i++) {
			xform = prop->prop_xforms + i;
			xform->xform_keylength =
			    keylength_xf(prop->prop_protoid,
			    xform->xform_type, xform->xform_id);

			switch (xform->xform_type) {
			case IKEV2_XFORMTYPE_ENCR:
			case IKEV2_XFORMTYPE_INTEGR:
				if (xform->xform_length)
					xform->xform_keylength =
					    xform->xform_length;
				xform->xform_keylength +=
				    noncelength_xf(xform->xform_type,
				    xform->xform_id);
				ilen += xform->xform_keylength / 8;
				break;
			}
		}
	}

	/* double key material length for inbound/outbound */
	ilen *= 2;

	log_debug("%s: key material length %zu", __func__, ilen);

	if ((seed = ibuf_new(NULL, 0)) == NULL) {
		log_debug("%s: failed to setup IKE SA key material", __func__);
		goto done;
	}
	if (pfs) {
		log_debug("%s: using PFS", __func__);
		if (kex->kex_dhpeer == NULL || ibuf_length(kex->kex_dhpeer) == 0 ||
		    (group = kex->kex_dhgroup) == NULL) {
			log_debug("%s: no dh group for pfs", __func__);
			goto done;
		}
		if ((dhsecret = ibuf_new(NULL, dh_getlen(group))) == NULL) {
			log_debug("%s: failed to alloc dh secret", __func__);
			goto done;
		}
		if (dh_create_shared(group, dhsecret->buf,
		    kex->kex_dhpeer->buf) == -1) {
			log_debug("%s: failed to get dh secret"
			    " group %d len %d secret %zu exchange %zu", __func__,
			    group->id, dh_getlen(group), ibuf_length(dhsecret),
			    ibuf_length(kex->kex_dhpeer));
			goto done;
		}
		if (ibuf_cat(seed, dhsecret) != 0) {
			log_debug("%s: failed to set dh secret", __func__);
			goto done;
		}
	}
	if (ibuf_cat(seed, kex->kex_inonce) != 0 ||
	    ibuf_cat(seed, kex->kex_rnonce) != 0 ||
	    (keymat = ikev2_prfplus(sa->sa_prf,
	    sa->sa_key_d, seed, ilen)) == NULL) {
		log_debug("%s: failed to get IKE SA key material", __func__);
		goto done;
	}

	/* Create the new flows */
	TAILQ_FOREACH(prop, proposals, prop_entry) {
		if (ikev2_valid_proposal(prop, NULL, NULL, NULL) != 0)
			continue;

		RB_FOREACH(flow, iked_flows, &sa->sa_policy->pol_flows) {
			skip = 0;
			TAILQ_FOREACH(saflow, &sa->sa_flows, flow_entry) {
				if (IKED_ADDR_EQ(&saflow->flow_src,
				    &flow->flow_src) &&
				    IKED_ADDR_EQ(&saflow->flow_dst,
				    &flow->flow_dst) &&
				    saflow->flow_saproto ==
				    prop->prop_protoid) {
					skip = 1;
					break;
				}
			}
			if (skip)
				continue;

			if ((flowa = calloc(1, sizeof(*flowa))) == NULL) {
				log_debug("%s: failed to get flow", __func__);
				goto done;
			}

			memcpy(flowa, flow, sizeof(*flow));
			flowa->flow_dir = IPSP_DIRECTION_OUT;
			flowa->flow_saproto = prop->prop_protoid;
			flowa->flow_local = &sa->sa_local;
			flowa->flow_peer = &sa->sa_peer;
			flowa->flow_ikesa = sa;
			ikev2_cp_fixaddr(sa, &flow->flow_dst, &flowa->flow_dst);

			if ((flowb = calloc(1, sizeof(*flowb))) == NULL) {
				log_debug("%s: failed to get flow", __func__);
				flow_free(flowa);
				goto done;
			}

			memcpy(flowb, flowa, sizeof(*flow));

			flowb->flow_dir = IPSP_DIRECTION_IN;
			memcpy(&flowb->flow_src, &flow->flow_dst,
			    sizeof(flow->flow_dst));
			memcpy(&flowb->flow_dst, &flow->flow_src,
			    sizeof(flow->flow_src));
			ikev2_cp_fixaddr(sa, &flow->flow_dst, &flowb->flow_src);

			TAILQ_INSERT_TAIL(&sa->sa_flows, flowa, flow_entry);
			TAILQ_INSERT_TAIL(&sa->sa_flows, flowb, flow_entry);
		}
	}

	/* create the CHILD SAs using the key material */
	TAILQ_FOREACH(prop, proposals, prop_entry) {
		if (ikev2_valid_proposal(prop, &encrxf, &integrxf, &esn) != 0)
			continue;

		spi = 0;

		if ((csa = calloc(1, sizeof(*csa))) == NULL) {
			log_debug("%s: failed to get CHILD SA", __func__);
			goto done;
		}

		csa->csa_saproto = prop->prop_protoid;
		csa->csa_ikesa = sa;
		csa->csa_spi.spi_protoid = prop->prop_protoid;
		csa->csa_esn = esn;

		/* Set up responder's SPIs */
		if (initiator) {
			csa->csa_dir = IPSP_DIRECTION_OUT;
			csa->csa_local = &sa->sa_local;
			csa->csa_peer = &sa->sa_peer;
			csa->csa_peerspi = prop->prop_localspi.spi;
			csa->csa_spi.spi = prop->prop_peerspi.spi;
			csa->csa_spi.spi_size = prop->prop_peerspi.spi_size;
		} else {
			csa->csa_dir = IPSP_DIRECTION_IN;
			csa->csa_local = &sa->sa_peer;
			csa->csa_peer = &sa->sa_local;

			if ((ret = pfkey_sa_init(env->sc_pfkey, csa,
			    &spi)) != 0)
				goto done;
			csa->csa_allocated = 1;

			csa->csa_peerspi = prop->prop_peerspi.spi;
			csa->csa_spi.spi = prop->prop_localspi.spi = spi;
			csa->csa_spi.spi_size = 4;
		}

		if (encrxf && (csa->csa_encrkey = ibuf_get(keymat,
		    encrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA encryption key",
			    __func__);
			childsa_free(csa);
			goto done;
		}
		if (integrxf && (csa->csa_integrkey = ibuf_get(keymat,
		    integrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA integrity key",
			    __func__);
			childsa_free(csa);
			goto done;
		}
		if (encrxf)
			csa->csa_encrid = encrxf->xform_id;
		if (integrxf)
			csa->csa_integrid = integrxf->xform_id;

		if ((csb = calloc(1, sizeof(*csb))) == NULL) {
			log_debug("%s: failed to get CHILD SA", __func__);
			childsa_free(csa);
			goto done;
		}

		memcpy(csb, csa, sizeof(*csb));

		/* Set up initiator's SPIs */
		csb->csa_spi.spi = csa->csa_peerspi;
		csb->csa_peerspi = csa->csa_spi.spi;
		csb->csa_allocated = csa->csa_allocated ? 0 : 1;
		csb->csa_dir = csa->csa_dir == IPSP_DIRECTION_IN ?
		    IPSP_DIRECTION_OUT : IPSP_DIRECTION_IN;
		csb->csa_local = csa->csa_peer;
		csb->csa_peer = csa->csa_local;

		if (encrxf && (csb->csa_encrkey = ibuf_get(keymat,
		    encrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA encryption key",
			    __func__);
			childsa_free(csa);
			childsa_free(csb);
			goto done;
		}
		if (integrxf && (csb->csa_integrkey = ibuf_get(keymat,
		    integrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA integrity key",
			    __func__);
			childsa_free(csa);
			childsa_free(csb);
			goto done;
		}

		TAILQ_INSERT_TAIL(&sa->sa_childsas, csa, csa_entry);
		TAILQ_INSERT_TAIL(&sa->sa_childsas, csb, csa_entry);

		csa->csa_peersa = csb;
		csb->csa_peersa = csa;
	}

	ret = 0;
 done:
	ibuf_release(dhsecret);
	ibuf_release(keymat);
	ibuf_release(seed);

	return (ret);
}

/* free a replaced IPCOMP SA */
void
ikev2_ipcomp_csa_free(struct iked *env, struct iked_childsa *csa)
{
	if (csa->csa_children)
		fatalx("ikev2_ipcomp_csa_free: has children");
	if (csa->csa_ikesa)
		TAILQ_REMOVE(&csa->csa_ikesa->sa_childsas, csa,
		    csa_entry);
	if (csa->csa_loaded) {
		log_debug("%s: csa %p loaded: calling pfkey_sa_delete",
		    __func__, csa);
		pfkey_sa_delete(env->sc_pfkey, csa);
		RB_REMOVE(iked_activesas, &env->sc_activesas, csa);
	}
	childsa_free(csa);
}

int
ikev2_ipcomp_enable(struct iked *env, struct iked_sa *sa)
{
	struct iked_childsa	*other, *nother, *csa = NULL, *csb = NULL;
	struct iked_flow	*flow, *flowa = NULL, *flowb = NULL;
	struct iked_flow	*nflow, *oflow;

	if ((csa = calloc(1, sizeof(*csa))) == NULL ||
	    (csb = calloc(1, sizeof(*csb))) == NULL ||
	    (flowa = calloc(1, sizeof(*flowa))) == NULL ||
	    (flowb = calloc(1, sizeof(*flowb))) == NULL) {
		free(csa);
		free(csb);
		free(flowa);
		free(flowb);
		return (-1);
	}

	/* switch ESP SAs to transport mode */
	TAILQ_FOREACH(other, &sa->sa_childsas, csa_entry) {
		if (!other->csa_rekey && !other->csa_loaded &&
		    other->csa_saproto == IKEV2_SAPROTO_ESP) {
			other->csa_transport = 1;
			if (other->csa_dir == IPSP_DIRECTION_OUT) {
				other->csa_parent = csa;
				csa->csa_children++;
			} else {
				other->csa_parent = csb;
				csb->csa_children++;
			}
		}
	}

	/* install IPCOMP SAs */
	csa->csa_ikesa = sa;
	csa->csa_saproto = IKEV2_SAPROTO_IPCOMP;
	csa->csa_spi.spi_size = 2;
	csa->csa_spi.spi = sa->sa_cpi_out;
	csa->csa_peerspi = sa->sa_cpi_in;
	csa->csa_dir = IPSP_DIRECTION_OUT;
	csa->csa_local = &sa->sa_local;
	csa->csa_peer = &sa->sa_peer;

	memcpy(csb, csa, sizeof(*csb));
	csb->csa_spi.spi = csa->csa_peerspi;
	csb->csa_peerspi = csa->csa_spi.spi;
	csb->csa_dir = IPSP_DIRECTION_IN;
	csb->csa_local = csa->csa_peer;
	csb->csa_peer = csa->csa_local;
	csb->csa_allocated = 1;

	/* remove old replaced IPCOMP SAs */
	TAILQ_FOREACH_SAFE(other, &sa->sa_childsas, csa_entry, nother) {
		if (other->csa_saproto != IKEV2_SAPROTO_IPCOMP ||
		    other->csa_children != 0)
			continue;
		if (other->csa_dir == csa->csa_dir &&
		    IKED_ADDR_EQ(other->csa_local, csa->csa_local) &&
		    IKED_ADDR_EQ(other->csa_peer, csa->csa_peer)) {
			log_debug("%s: csa %p replaces %p",
			    __func__, csa, other);
			ikev2_ipcomp_csa_free(env, other);
		} else if (other->csa_dir == csb->csa_dir &&
		    IKED_ADDR_EQ(other->csa_local, csb->csa_local) &&
		    IKED_ADDR_EQ(other->csa_peer, csb->csa_peer)) {
			log_debug("%s: csa %p replaces %p",
			    __func__, csb, other);
			ikev2_ipcomp_csa_free(env, other);
		}
	}

	TAILQ_INSERT_TAIL(&sa->sa_childsas, csa, csa_entry);
	TAILQ_INSERT_TAIL(&sa->sa_childsas, csb, csa_entry);

	csa->csa_peersa = csb;
	csb->csa_peersa = csa;

	/* redirect flows to IPCOMP */
	/* XXX expensive? should be merged into ikev2_childsa_negotiate() */
	TAILQ_FOREACH_SAFE(flow, &sa->sa_flows, flow_entry, nflow) {
		if (flow->flow_loaded ||
		    flow->flow_saproto != IKEV2_SAPROTO_ESP)
			continue;
		TAILQ_FOREACH(oflow, &sa->sa_flows, flow_entry)
			if (IKED_ADDR_EQ(&oflow->flow_src, &flow->flow_src) &&
			    IKED_ADDR_EQ(&oflow->flow_dst, &flow->flow_dst) &&
			    oflow->flow_dir == flow->flow_dir &&
			    oflow->flow_saproto == IKEV2_SAPROTO_IPCOMP)
				break;
		if (oflow != NULL) {
			log_debug("%s: keeping oflow %p, indentical to flow %p",
			    __func__, oflow, flow);
			TAILQ_REMOVE(&sa->sa_flows, flow, flow_entry);
			flow_free(flow);
		} else {
			log_debug("%s: flow %p saproto %d -> %d", __func__,
			    flow, flow->flow_saproto, IKEV2_SAPROTO_IPCOMP);
			flow->flow_saproto = IKEV2_SAPROTO_IPCOMP;
		}
	}

	/* setup ESP flows for gateways */
	flowa->flow_dir = IPSP_DIRECTION_OUT;
	flowa->flow_saproto = IKEV2_SAPROTO_ESP;
	flowa->flow_local = &sa->sa_local;
	flowa->flow_peer = &sa->sa_peer;
	memcpy(&flowa->flow_src, &sa->sa_local, sizeof(sa->sa_local));
	memcpy(&flowa->flow_dst, &sa->sa_peer, sizeof(sa->sa_peer));
	socket_setport((struct sockaddr *)&flowa->flow_src.addr, 0);
	socket_setport((struct sockaddr *)&flowa->flow_dst.addr, 0);
	flowa->flow_src.addr_port = flowa->flow_dst.addr_port = 0;
	flowa->flow_src.addr_mask = flowa->flow_dst.addr_mask =
	    (sa->sa_local.addr_af == AF_INET) ? 32 : 128;
	flowa->flow_ikesa = sa;

	/* skip if flow already exists */
	TAILQ_FOREACH(flow, &sa->sa_flows, flow_entry) {
		if (IKED_ADDR_EQ(&flow->flow_src, &flowa->flow_src) &&
		    IKED_ADDR_EQ(&flow->flow_dst, &flowa->flow_dst) &&
		    flow->flow_dir == flowa->flow_dir &&
		    flow->flow_saproto == flowa->flow_saproto) {
			free(flowa);
			free(flowb);
			goto done;
		}
	}

	memcpy(flowb, flowa, sizeof(*flowb));
	flowb->flow_dir = IPSP_DIRECTION_IN;
	memcpy(&flowb->flow_dst, &flowa->flow_src, sizeof(flowa->flow_src));
	memcpy(&flowb->flow_src, &flowa->flow_dst, sizeof(flowa->flow_dst));

	TAILQ_INSERT_TAIL(&sa->sa_flows, flowa, flow_entry);
	TAILQ_INSERT_TAIL(&sa->sa_flows, flowb, flow_entry);

 done:
	/* make sure IPCOMP CPIs are not reused */
	sa->sa_ipcomp = 0;
	sa->sa_cpi_in = sa->sa_cpi_out = 0;

	return (0);
}

int
ikev2_childsa_enable(struct iked *env, struct iked_sa *sa)
{
	struct iked_childsa	*csa;
	struct iked_flow	*flow, *oflow;

	if (sa->sa_ipcomp && sa->sa_cpi_in && sa->sa_cpi_out &&
	    ikev2_ipcomp_enable(env, sa) == -1)
		return (-1);

	TAILQ_FOREACH(csa, &sa->sa_childsas, csa_entry) {
		if (csa->csa_rekey || csa->csa_loaded)
			continue;

		if (pfkey_sa_add(env->sc_pfkey, csa, NULL) != 0) {
			log_debug("%s: failed to load CHILD SA spi %s",
			    __func__, print_spi(csa->csa_spi.spi,
			    csa->csa_spi.spi_size));
			return (-1);
		}

		RB_INSERT(iked_activesas, &env->sc_activesas, csa);

		log_debug("%s: loaded CHILD SA spi %s", __func__,
		    print_spi(csa->csa_spi.spi, csa->csa_spi.spi_size));
	}

	TAILQ_FOREACH(flow, &sa->sa_flows, flow_entry) {
		if (flow->flow_loaded)
			continue;

		if (pfkey_flow_add(env->sc_pfkey, flow) != 0) {
			log_debug("%s: failed to load flow", __func__);
			return (-1);
		}

		if ((oflow = RB_FIND(iked_flows, &env->sc_activeflows, flow))
		    != NULL) {
			log_debug("%s: replaced old flow %p with %p",
			    __func__, oflow, flow);
			oflow->flow_loaded = 0;
			RB_REMOVE(iked_flows, &env->sc_activeflows, oflow);
		}

		RB_INSERT(iked_flows, &env->sc_activeflows, flow);

		log_debug("%s: loaded flow %p", __func__, flow);
	}

	return (0);
}

int
ikev2_childsa_delete(struct iked *env, struct iked_sa *sa, u_int8_t saproto,
    u_int64_t spi, u_int64_t *spiptr, int cleanup)
{
	struct iked_childsa	*csa, *nextcsa = NULL;
	u_int64_t		 peerspi = 0;
	int			 found = 0;

	for (csa = TAILQ_FIRST(&sa->sa_childsas); csa != NULL; csa = nextcsa) {
		nextcsa = TAILQ_NEXT(csa, csa_entry);

		if ((saproto && csa->csa_saproto != saproto) ||
		    (spi && (csa->csa_spi.spi != spi &&
			     csa->csa_peerspi != spi)) ||
		    (cleanup && csa->csa_loaded))
			continue;

		if (csa->csa_loaded)
			RB_REMOVE(iked_activesas, &env->sc_activesas, csa);

		if (pfkey_sa_delete(env->sc_pfkey, csa) != 0)
			log_debug("%s: failed to delete CHILD SA spi %s",
			    __func__, print_spi(csa->csa_spi.spi,
			    csa->csa_spi.spi_size));
		else
			log_debug("%s: deleted CHILD SA spi %s", __func__,
			    print_spi(csa->csa_spi.spi,
			    csa->csa_spi.spi_size));
		found++;

		if (spi && csa->csa_spi.spi == spi)
			peerspi = csa->csa_peerspi;

		TAILQ_REMOVE(&sa->sa_childsas, csa, csa_entry);
		childsa_free(csa);
	}

	if (spiptr)
		*spiptr = peerspi;

	return (found ? 0 : -1);
}

int
ikev2_valid_proposal(struct iked_proposal *prop,
    struct iked_transform **exf, struct iked_transform **ixf, int *esn)
{
	struct iked_transform	*xform, *encrxf, *integrxf;
	u_int			 i, doesn = 0;

	switch (prop->prop_protoid) {
	case IKEV2_SAPROTO_ESP:
	case IKEV2_SAPROTO_AH:
		break;
	default:
		return (-1);
	}

	encrxf = integrxf = NULL;
	for (i = 0; i < prop->prop_nxforms; i++) {
		xform = prop->prop_xforms + i;
		if (xform->xform_type == IKEV2_XFORMTYPE_ENCR)
			encrxf = xform;
		else if (xform->xform_type == IKEV2_XFORMTYPE_INTEGR)
			integrxf = xform;
		else if (xform->xform_type == IKEV2_XFORMTYPE_ESN &&
		    xform->xform_id == IKEV2_XFORMESN_ESN)
			doesn = 1;
	}

	if (prop->prop_protoid == IKEV2_SAPROTO_IKE) {
		if (encrxf == NULL || integrxf == NULL)
			return (-1);
	} else if (prop->prop_protoid == IKEV2_SAPROTO_AH) {
		if (integrxf == NULL)
			return (-1);
	} else if (prop->prop_protoid == IKEV2_SAPROTO_ESP) {
		if (encrxf == NULL)
			return (-1);
	}

	if (exf)
		*exf = encrxf;
	if (ixf)
		*ixf = integrxf;
	if (esn)
		*esn = doesn;

	return (0);
}

/* return 0 if processed, -1 if busy */
int
ikev2_acquire_sa(struct iked *env, struct iked_flow *acquire)
{
	struct iked_flow	*flow;
	struct iked_sa		*sa;
	struct iked_policy	 pol, *p = NULL;

	if (env->sc_passive)
		return (0);

	/* First try to find an active flow with IKE SA */
	flow = RB_FIND(iked_flows, &env->sc_activeflows, acquire);
	if (!flow) {
		/* Otherwise try to find a matching policy */
		bzero(&pol, sizeof(pol));
		pol.pol_af = acquire->flow_peer->addr_af;
		memcpy(&pol.pol_peer, acquire->flow_peer,
		    sizeof(pol.pol_peer));

		RB_INIT(&pol.pol_flows);
		RB_INSERT(iked_flows, &pol.pol_flows, acquire);
		pol.pol_nflows = 1;

		if ((p = policy_test(env, &pol)) == NULL) {
			log_warnx("%s: flow wasn't found", __func__);
			return (0);
		}

		log_debug("%s: found matching policy '%s'", __func__,
		    p->pol_name);

		if (ikev2_init_ike_sa_peer(env, p, acquire->flow_peer) != 0)
			log_warnx("%s: failed to initiate a "
			    "IKE_SA_INIT exchange", __func__);
	} else {
		log_debug("%s: found active flow", __func__);

		if ((sa = flow->flow_ikesa) == NULL) {
			log_warnx("%s: flow without SA", __func__);
			return (0);
		}
		if (sa->sa_stateflags & IKED_REQ_CHILDSA)
			return (-1);	/* busy, retry later */
		if (ikev2_send_create_child_sa(env, sa, NULL,
		    flow->flow_saproto) != 0)
			log_warnx("%s: failed to initiate a "
			    "CREATE_CHILD_SA exchange", __func__);
	}
	return (0);
}

void
ikev2_disable_rekeying(struct iked *env, struct iked_sa *sa)
{
	struct iked_childsa		*csa;

	TAILQ_FOREACH(csa, &sa->sa_childsas, csa_entry) {
		csa->csa_persistent = 1;
		csa->csa_rekey = 0;
	}

	(void)ikev2_childsa_delete(env, sa, 0, 0, NULL, 1);
}

/* return 0 if processed, -1 if busy */
int
ikev2_rekey_sa(struct iked *env, struct iked_spi *rekey)
{
	struct iked_childsa		*csa, key;
	struct iked_sa			*sa;

	key.csa_spi = *rekey;
	csa = RB_FIND(iked_activesas, &env->sc_activesas, &key);
	if (!csa)
		return (0);

	if (csa->csa_rekey)	/* See if it's already taken care of */
		return (0);
	if (csa->csa_saproto == IKEV2_SAPROTO_IPCOMP)	/* no rekey */
		return (0);
	if ((sa = csa->csa_ikesa) == NULL) {
		log_warnx("%s: SA %s doesn't have a parent SA", __func__,
		    print_spi(rekey->spi, rekey->spi_size));
		return (0);
	}
	if (!sa_stateok(sa, IKEV2_STATE_ESTABLISHED)) {
		log_warnx("%s: SA %s is not established", __func__,
		    print_spi(rekey->spi, rekey->spi_size));
		return (0);
	}
	if (sa->sa_stateflags & IKED_REQ_CHILDSA)
		return (-1);	/* busy, retry later */
	if (csa->csa_allocated)	/* Peer SPI died first, get the local one */
		rekey->spi = csa->csa_peerspi;
	if (ikev2_send_create_child_sa(env, sa, rekey, rekey->spi_protoid))
		log_warnx("%s: failed to initiate a CREATE_CHILD_SA exchange",
		    __func__);
	return (0);
}

/* return 0 if processed, -1 if busy */
int
ikev2_drop_sa(struct iked *env, struct iked_spi *drop)
{
	struct ibuf			*buf = NULL;
	struct iked_childsa		*csa, key;
	struct iked_sa			*sa;
	struct ikev2_delete		*del;
	u_int32_t			 spi32;

	key.csa_spi = *drop;
	csa = RB_FIND(iked_activesas, &env->sc_activesas, &key);
	if (!csa || csa->csa_rekey)
		return (0);

	sa = csa->csa_ikesa;
	if (sa && (sa->sa_stateflags & IKED_REQ_CHILDSA))
		return (-1);	/* busy, retry later */

	RB_REMOVE(iked_activesas, &env->sc_activesas, csa);
	csa->csa_loaded = 0;
	csa->csa_rekey = 1;	/* prevent re-loading */
	if (sa == NULL) {
		log_debug("%s: failed to find a parent SA", __func__);
		return (0);
	}
	if (csa->csa_saproto == IKEV2_SAPROTO_IPCOMP) {
		/* matching Child SAs (e.g. ESP) should have expired by now */
		if (csa->csa_children == 0)
			ikev2_ipcomp_csa_free(env, csa);
		return (0);
	}

	if (csa->csa_allocated)
		spi32 = htobe32(csa->csa_spi.spi);
	else
		spi32 = htobe32(csa->csa_peerspi);

	if (ikev2_childsa_delete(env, sa, csa->csa_saproto,
	    csa->csa_peerspi, NULL, 0))
		log_debug("%s: failed to delete CHILD SA %s", __func__,
		    print_spi(csa->csa_peerspi, drop->spi_size));

	/* Send PAYLOAD_DELETE */

	if ((buf = ibuf_static()) == NULL)
		return (0);
	if ((del = ibuf_advance(buf, sizeof(*del))) == NULL)
		goto done;
	del->del_protoid = drop->spi_protoid;
	del->del_spisize = 4;
	del->del_nspi = htobe16(1);
	if (ibuf_add(buf, &spi32, sizeof(spi32)))
		goto done;

	if (ikev2_send_ike_e(env, sa, buf, IKEV2_PAYLOAD_DELETE,
	    IKEV2_EXCHANGE_INFORMATIONAL, 0) == -1)
		goto done;

	sa->sa_stateflags |= IKED_REQ_INF;

	/* Initiate Child SA creation */
	if (ikev2_send_create_child_sa(env, sa, NULL, drop->spi_protoid))
		log_warnx("%s: failed to initiate a CREATE_CHILD_SA exchange",
		    __func__);

done:
	ibuf_release(buf);
	return (0);
}

int
ikev2_print_id(struct iked_id *id, char *idstr, size_t idstrlen)
{
	u_int8_t			 buf[BUFSIZ], *ptr;
	struct sockaddr_in		*s4;
	struct sockaddr_in6		*s6;
	char				*str;
	ssize_t				 len;
	int				 i;
	const char			*type;

	bzero(buf, sizeof(buf));
	bzero(idstr, idstrlen);

	if (id->id_buf == NULL)
		return (-1);

	len = ibuf_size(id->id_buf);
	ptr = ibuf_data(id->id_buf);

	if (len <= id->id_offset)
		return (-1);

	len -= id->id_offset;
	ptr += id->id_offset;

	type = print_map(id->id_type, ikev2_id_map);

	if (strlcpy(idstr, type, idstrlen) >= idstrlen ||
	    strlcat(idstr, "/", idstrlen) >= idstrlen)
		return (-1);

	idstr += strlen(idstr);
	idstrlen -= strlen(idstr);

	switch (id->id_type) {
	case IKEV2_ID_IPV4:
		s4 = (struct sockaddr_in *)buf;
		s4->sin_family = AF_INET;
		s4->sin_len = sizeof(*s4);
		memcpy(&s4->sin_addr.s_addr, ptr, len);

		if (print_host((struct sockaddr *)s4,
		    idstr, idstrlen) == NULL)
			return (-1);
		break;
	case IKEV2_ID_FQDN:
	case IKEV2_ID_UFQDN:
		if (len >= (ssize_t)sizeof(buf))
			return (-1);

		if ((str = get_string(ptr, len)) == NULL)
			return (-1);

		if (strlcpy(idstr, str, idstrlen) >= idstrlen) {
			free(str);
			return (-1);
		}
		free(str);
		break;
	case IKEV2_ID_IPV6:
		s6 = (struct sockaddr_in6 *)buf;
		s6->sin6_family = AF_INET6;
		s6->sin6_len = sizeof(*s6);
		memcpy(&s6->sin6_addr, ptr, len);

		if (print_host((struct sockaddr *)s6,
		    idstr, idstrlen) == NULL)
			return (-1);
		break;
	case IKEV2_ID_ASN1_DN:
		if ((str = ca_asn1_name(ptr, len)) == NULL)
			return (-1);
		if (strlcpy(idstr, str, idstrlen) >= idstrlen) {
			free(str);
			return (-1);
		}
		free(str);
		break;
	default:
		/* XXX test */
		for (i = 0; i < ((ssize_t)idstrlen - 1) && i < len; i++)
			snprintf(idstr + i, idstrlen - i,
			    "%02x", ptr[i]);
		break;
	}

	return (0);
}

/*
 * If we have an IKEV2_CP_REQUEST for IKEV2_CFG_INTERNAL_IP4_ADDRESS and
 * if a network(pool) is configured, then select an address from that pool
 * and remember it in the sa_addrpool attribute.
 */
int
ikev2_cp_setaddr(struct iked *env, struct iked_sa *sa)
{
	struct iked_cfg		*ikecfg = NULL;
	struct iked_policy	*pol = sa->sa_policy;
	struct sockaddr_in	*in4 = NULL, *cfg4 = NULL;
	struct sockaddr_in6	*in6 = NULL, *cfg6 = NULL;
	struct iked_sa		 key;
	struct iked_addr	 addr;
	u_int32_t		 mask, host, lower, upper, start;
	size_t			 i;

	if (sa->sa_addrpool || pol->pol_ncfg == 0)
		return (0);
	/* check for an address pool config (address w/ prefixlen != 32) */
	bzero(&addr, sizeof(addr));
	for (i = 0; i < pol->pol_ncfg; i++) {
		ikecfg = &pol->pol_cfg[i];
		if (ikecfg->cfg_type == IKEV2_CFG_INTERNAL_IP4_ADDRESS &&
		    ikecfg->cfg.address.addr_mask != 32) {
			addr.addr_af = AF_INET;
			break;
		}
		if (ikecfg->cfg_type == IKEV2_CFG_INTERNAL_IP6_ADDRESS &&
		    ikecfg->cfg.address.addr_mask != 128) {
			addr.addr_af = AF_INET6;
			break;
		}
	}
	if (i == pol->pol_ncfg)
		return (0);

	/*
	 * failure: pool configured, but not requested.
	 * If we continue, we might end up with flows where 0.0.0.0 is NOT
	 * replaced with an address from the pool with ikev2_cp_fixaddr().
	 */
	if (sa->sa_cp != IKEV2_CP_REQUEST) {
		log_debug("%s: pool configured, but IKEV2_CP_REQUEST missing",
		    __func__);
		return (-1);
	}

	/* truncate prefixlen in the v6 case */
	mask = prefixlen2mask(ikecfg->cfg.address.addr_mask);

	switch (addr.addr_af) {
	case AF_INET:
		cfg4 = (struct sockaddr_in *)&ikecfg->cfg.address.addr;
		in4 = (struct sockaddr_in *)&addr.addr;
		in4->sin_family = AF_INET;
		in4->sin_len = sizeof(*in4);
		lower = ntohl(cfg4->sin_addr.s_addr & ~mask);
		break;
	case AF_INET6:
		cfg6 = (struct sockaddr_in6 *)&ikecfg->cfg.address.addr;
		in6 = (struct sockaddr_in6 *)&addr.addr;
		in6->sin6_family = AF_INET6;
		in6->sin6_len = sizeof(*in6);
		lower = cfg6->sin6_addr.s6_addr[3];
		break;
	default:
		return (-1);
	}

	if (lower == 0)
		lower = 1;
	/* Note that start, upper and host are in HOST byte order */
	upper = ntohl(~mask);
	/* Randomly select start from [lower, upper-1] */
	start = arc4random_uniform(upper - lower) + lower;

	key.sa_addrpool = &addr;

	for (host = start;;) {
		log_debug("%s: mask %x start %x lower %x host %x upper %x",
		    __func__, mask, start, lower, host, upper);
		switch (addr.addr_af) {
		case AF_INET:
			in4->sin_addr.s_addr =
			    (cfg4->sin_addr.s_addr & mask) | htonl(host);
			break;
		case AF_INET6:
			memcpy(in6, cfg6, sizeof(*in6));
			in6->sin6_addr.s6_addr[3] = htonl(host);
			break;
		}
		if (!RB_FIND(iked_addrpool, &env->sc_addrpool, &key))
			break;
		/* try next address */
		host++;
		/* but skip broadcast and network address */
		if (host >= upper || host < lower)
			host = lower;
		if (host == start)
			return (-1);		/* exhausted */
	}
	if (!key.sa_addrpool)
		return (-1);			/* cannot happen? */
	if ((sa->sa_addrpool = calloc(1, sizeof(addr))) == NULL)
		return (-1);
	memcpy(sa->sa_addrpool, &addr, sizeof(addr));
	RB_INSERT(iked_addrpool, &env->sc_addrpool, sa);
	return (0);
}

/*
 * if 'addr' is 'UNSPECIFIED' replace it with sa_addrpool from
 * the ip-pool and store the result in 'patched'.
 */
int
ikev2_cp_fixaddr(struct iked_sa *sa, struct iked_addr *addr,
    struct iked_addr *patched)
{
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;

	if (sa->sa_addrpool == NULL ||
	    sa->sa_addrpool->addr_af != addr->addr_af)
		return (-1);
	switch (addr->addr_af) {
	case AF_INET:
		in4 = (struct sockaddr_in *)&addr->addr;
		if (in4->sin_addr.s_addr)
			return (-1);
		break;
	case AF_INET6:
		in6 = (struct sockaddr_in6 *)&addr->addr;
		if (IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr))
			return (-1);
		break;
	}
	memcpy(patched, sa->sa_addrpool, sizeof(*patched));
	return (0);
}

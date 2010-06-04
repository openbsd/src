/*	$OpenBSD: ikev2.c,v 1.2 2010/06/04 09:51:45 reyk Exp $	*/
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

int	 ikev2_dispatch_parent(int, struct iked_proc *, struct imsg *);
int	 ikev2_dispatch_ikev1(int, struct iked_proc *, struct imsg *);
int	 ikev2_dispatch_cert(int, struct iked_proc *, struct imsg *);

struct iked_sa *
	 ikev2_getimsgdata(struct iked *, struct imsg *, struct iked_sahdr *,
	    u_int8_t *, u_int8_t **, size_t *);

void	 ikev2_message_cb(int, short, void *);
void	 ikev2_message_recv(struct iked *, struct iked_message *);
int	 ikev2_message_valid_ike_sa(struct iked *, struct ike_header *,
	    struct iked_message *);
int	 ikev2_message_send(struct iked *, int, struct iked_message *);
int	 ikev2_message_send_encrypt(struct iked *, struct iked_sa *,
	    struct ibuf **, u_int8_t, u_int8_t, int);
u_int32_t
	 ikev2_message_id(struct iked *, struct iked_sa *, int);
struct ibuf *
	 ikev2_message_init(struct iked *, struct iked_message *,
	    struct sockaddr_storage *, socklen_t,
	    struct sockaddr_storage *, socklen_t, int);
struct ibuf
	*ikev2_message_auth(struct iked *, struct iked_sa *, int);
int	 ikev2_message_authverify(struct iked *, struct iked_sa *,
	    struct iked_auth *, u_int8_t *, size_t, struct ibuf *);

struct ibuf
	*ikev2_message_encrypt(struct iked *, struct iked_sa *, struct ibuf *);
struct ibuf *
	 ikev2_message_decrypt(struct iked *, struct iked_sa *,
	    struct ibuf *, struct ibuf *);
int	 ikev2_message_integr(struct iked *, struct iked_sa *, struct ibuf *);

int	 ikev2_resp_ike_sa_init(struct iked *, struct iked_message *);
int	 ikev2_resp_ike_auth(struct iked *, struct iked_sa *);
int	 ikev2_resp_ike_eap(struct iked *, struct iked_sa *, struct ibuf *);
int	 ikev2_resp_create_child_sa(struct iked *, struct iked_message *);
int	 ikev2_send_informational(struct iked *, struct iked_message *);

int	 ikev2_parse_message(struct iked *, struct ike_header *,
	    struct iked_message *, off_t);
int	 ikev2_parse_payloads(struct iked *, struct iked_message *,
	    off_t, size_t, u_int, int);
int	 ikev2_parse_sa(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_xform(struct iked *, struct ikev2_sa_proposal *,
	    struct iked_message *, off_t);
int	 ikev2_parse_attr(struct iked *, struct ikev2_transform *,
	    struct iked_message *, off_t, int);
int	 ikev2_parse_ke(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_id(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t, u_int);
int	 ikev2_parse_cert(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_certreq(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_nonce(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_notify(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_delete(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_ts(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t, u_int);
int	 ikev2_parse_auth(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_e(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_cp(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);
int	 ikev2_parse_eap(struct iked *, struct ikev2_payload *,
	    struct iked_message *, off_t);

int	 ikev2_policy2id(struct iked_static_id *, struct iked_id *, int);
int	 ikev2_sa_negotiate(struct iked_sa *, struct iked_proposals *,
	    struct iked_proposals *, u_int8_t);
int	 ikev2_sa_keys(struct iked_sa *);
int	 ikev2_sa_tag(struct iked_sa *, struct iked_id *);
int	 ikev2_childsa_negotiate(struct iked *, struct iked_sa *,
	    struct iked_spi *);
int	 ikev2_childsa_enable(struct iked *, struct iked_sa *);
int	 ikev2_childsa_delete(struct iked *, struct iked_sa *,
	    u_int8_t, u_int64_t, u_int64_t *, int);
int	 ikev2_valid_proposal(struct iked_proposal *,
	    struct iked_transform **, struct iked_transform **);

struct ike_header *
	 ikev2_add_header(struct ibuf *, struct iked_sa *,
	    u_int32_t, u_int8_t, u_int8_t, u_int8_t);
int	 ikev2_set_header(struct ike_header *, size_t);
struct ikev2_payload *
	 ikev2_add_payload(struct ibuf *);
int	 ikev2_next_payload(struct ikev2_payload *, size_t,
	    u_int8_t);
ssize_t	 ikev2_add_proposals(struct ibuf *, struct iked_proposals *, u_int8_t);
ssize_t	 ikev2_nat_detection(struct iked_message *, void *, size_t,
	    u_int, int);
ssize_t	 ikev2_add_cp(struct iked *, struct iked_sa *, struct ibuf *);
ssize_t	 ikev2_add_transform(struct ibuf *,
	    u_int8_t, u_int8_t, u_int16_t, u_int16_t);
ssize_t	 ikev2_add_ts(struct ibuf *, u_int, struct iked_sa *);
int	 ikev2_add_data(struct ibuf *, void *, size_t);
int	 ikev2_add_buf(struct ibuf *buf, struct ibuf *);

static struct iked_proc procs[] = {
	{ "parent",	PROC_PARENT,	ikev2_dispatch_parent },
	{ "ikev1",	PROC_IKEV1,	ikev2_dispatch_ikev1 },
	{ "certstore",	PROC_CERT,	ikev2_dispatch_cert }
};

pid_t
ikev2(struct iked *env, struct iked_proc *p)
{
	return (run_proc(env, p, procs, nitems(procs), NULL, NULL));
}

int
ikev2_dispatch_parent(int fd, struct iked_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->env;

	switch (imsg->hdr.type) {
	case IMSG_CTL_RESET:
		return (config_getreset(env, imsg));
	case IMSG_UDP_SOCKET:
		return (config_getsocket(env, imsg, ikev2_message_cb));
	case IMSG_PFKEY_SOCKET:
		return (config_getpfkey(env, imsg));
	case IMSG_CFG_POLICY:
		return (config_getpolicy(env, imsg));
	case IMSG_CFG_USER:
		return (config_getuser(env, imsg));
	default:
		break;
	}

	return (-1);
}

int
ikev2_dispatch_ikev1(int fd, struct iked_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->env;
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

		log_debug("%s: message length %d", __func__, len);

		ikev2_message_recv(env, &msg);
		message_cleanup(env, &msg);
		return (0);
	default:
		break;
	}

	return (-1);
}

int
ikev2_dispatch_cert(int fd, struct iked_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->env;
	struct iked_sahdr	 sh;
	struct iked_sa		*sa;
	u_int8_t		 type;
	u_int8_t		*ptr;
	size_t			 len;
	struct iked_id		*id = NULL;

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

		log_debug("%s: updated local CERTREQ signatures length %d",
		    __func__, ibuf_length(env->sc_certreq));

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
			sa_state(env, sa, IKEV2_STATE_VALID);
		} else
			log_debug("%s: peer certificate is invalid", __func__);

		sa_stateflags(sa, IKED_REQ_VALID);

		if (ikev2_resp_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send auth response", __func__);
		break;
	case IMSG_CERT:
		if ((sa = ikev2_getimsgdata(env, imsg,
		    &sh, &type, &ptr, &len)) == NULL) {
			log_debug("%s: invalid cert reply");
			break;
		}

		if (sh.sh_initiator)
			id = &sa->sa_icert;
		else
			id = &sa->sa_rcert;

		id->id_type = type;
		ibuf_release(id->id_buf);
		id->id_buf = NULL;

		if (type != IKEV2_CERT_NONE) {
			if (len <= 0 ||
			    (id->id_buf = ibuf_new(ptr, len)) == NULL) {
				log_debug("%s: failed to get cert payload",
				    __func__);
				break;
			}
		}

		log_debug("%s: cert type %d length %d", __func__,
		    id->id_type, ibuf_length(id->id_buf));

		sa_stateflags(sa, IKED_REQ_CERT);

		if (ikev2_resp_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send auth response", __func__);
		break;
	case IMSG_AUTH:
		if ((sa = ikev2_getimsgdata(env, imsg,
		    &sh, &type, &ptr, &len)) == NULL) {
			log_debug("%s: invalid auth reply", __func__);
			break;
		}

		log_debug("%s: AUTH type %d len %d", __func__, type, len);

		id = &sa->sa_localauth;
		id->id_type = type;
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

		if (ikev2_resp_ike_auth(env, sa) != 0)
			log_debug("%s: failed to send auth response", __func__);
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
	    " type %d data length %d",
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
ikev2_message_cb(int fd, short event, void *arg)
{
	struct iked_socket	*sock = arg;
	struct iked		*env = sock->sock_env;
	struct iked_message	 msg;
	struct ike_header	 hdr;
	u_int32_t		 natt = 0x00000000;
	u_int8_t		 buf[IKED_MSGBUF_MAX];
	ssize_t			 len;
	off_t			 off;
	struct iovec		 iov[2];

	bzero(&msg, sizeof(msg));
	bzero(buf, sizeof(buf));

	msg.msg_peerlen = sizeof(msg.msg_peer);
	msg.msg_locallen = sizeof(msg.msg_local);
	memcpy(&msg.msg_local, &sock->sock_addr, sizeof(sock->sock_addr));

	if ((len = recvfromto(fd, buf, sizeof(buf), 0,
	    (struct sockaddr*)&msg.msg_peer, &msg.msg_peerlen,
	    (struct sockaddr*)&msg.msg_local, &msg.msg_locallen)) <
	    (ssize_t)sizeof(natt))
		return;

	if (socket_getport(&msg.msg_local) == IKED_NATT_PORT) {
		if (bcmp(&natt, buf, sizeof(natt)) != 0)
			return;
		msg.msg_natt = 1;
		off = sizeof(natt);
	} else
		off = 0;

	if ((size_t)(len - off) <= sizeof(hdr))
		return;
	memcpy(&hdr, buf + off, sizeof(hdr));

	if ((msg.msg_data = ibuf_new(buf + off, len - off)) == NULL)
		return;

	if (hdr.ike_version == IKEV1_VERSION) {
		iov[0].iov_base = &msg;
		iov[0].iov_len = sizeof(msg);
		iov[1].iov_base = buf;
		iov[1].iov_len = len;

		imsg_composev_proc(env, PROC_IKEV1, IMSG_IKE_MESSAGE, -1,
		    iov, 2);
		goto done;
	}
	TAILQ_INIT(&msg.msg_proposals);

	msg.msg_fd = fd;
	ikev2_message_recv(env, &msg);

 done:
	message_cleanup(env, &msg);
}

void
ikev2_message_recv(struct iked *env, struct iked_message *msg)
{
	u_int			 initiator;
	struct iked_sa		*sa;
	struct ike_header	*hdr;

	hdr = ibuf_seek(msg->msg_data, msg->msg_offset, sizeof(*hdr));

	if (hdr == NULL || (ssize_t)ibuf_size(msg->msg_data) <
	    (betoh32(hdr->ike_length) - msg->msg_offset))
		return;

	initiator = (hdr->ike_flags & IKEV2_FLAG_INITIATOR) ? 0 : 1;
	msg->msg_sa = sa_lookup(env,
	    betoh64(hdr->ike_ispi), betoh64(hdr->ike_rspi),
	    initiator);
	if (policy_lookup(env, msg) != 0)
		return;

	log_info("%s: %s from %s to %s policy '%s', %ld bytes", __func__,
	    print_map(hdr->ike_exchange, ikev2_exchange_map),
	    print_host(&msg->msg_peer, NULL, 0),
	    print_host(&msg->msg_local, NULL, 0),
	    msg->msg_policy->pol_name,
	    ibuf_size(msg->msg_data));

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		if (msg->msg_sa != NULL) {
			log_debug("%s: SA already exists", __func__);
			return;
		}
		if ((msg->msg_sa = sa_new(env,
		    betoh64(hdr->ike_ispi), betoh64(hdr->ike_rspi),
		    initiator, msg->msg_policy)) == NULL) {
			log_debug("%s: failed to get new SA", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
		if (ikev2_message_valid_ike_sa(env, hdr, msg) == -1)
			return;
		if (sa_stateok(msg->msg_sa, IKEV2_STATE_VALID)) {
			log_debug("%s: already authenticated", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		if (ikev2_message_valid_ike_sa(env, hdr, msg) == -1)
			return;
		break;
	case IKEV2_EXCHANGE_INFORMATIONAL:
		if (ikev2_message_valid_ike_sa(env, hdr, msg) == -1)
			return;
		break;
	default:
		log_debug("%s: unsupported exchange: %s", __func__,
		    print_map(hdr->ike_exchange, ikev2_exchange_map));
		return;
	}

	if (msg->msg_sa != NULL) {
		sa = msg->msg_sa;

		log_debug("%s: updating msg, natt %d", __func__, msg->msg_natt);

		if (msg->msg_natt)
			sa->sa_natt = 1;

		if (betoh32(hdr->ike_msgid) < sa->sa_msgid) {
			log_debug("%s: invalid sequence number", __func__);
			return;
		} else
			sa->sa_msgid = betoh32(hdr->ike_msgid);

		if (msg->msg_peerlen > sizeof(sa->sa_peer)) {
			log_debug("%s: invalid peer address",
			    __func__);
			return;
		}
		sa->sa_peer.addr_af = msg->msg_peer.ss_family;
		sa->sa_peer.addr.ss_len = msg->msg_peerlen;
		sa->sa_peer.addr_port =
		    htons(socket_getport(&msg->msg_peer));
		socket_af((struct sockaddr *)&sa->sa_peer.addr,
		    sa->sa_peer.addr_port);
		memcpy(&sa->sa_peer.addr, &msg->msg_peer,
		    sizeof(sa->sa_peer.addr));

		if (msg->msg_locallen > sizeof(sa->sa_local)) {
			log_debug("%s: invalid local address",
			    __func__);
			return;
		}
		sa->sa_local.addr_af = msg->msg_local.ss_family;
		sa->sa_local.addr.ss_len = msg->msg_locallen;
		sa->sa_local.addr_port =
		    htons(socket_getport(&msg->msg_local));
		socket_af((struct sockaddr *)&sa->sa_local.addr,
		    sa->sa_local.addr_port);
		memcpy(&sa->sa_local.addr, &msg->msg_local,
		    sizeof(sa->sa_local.addr));
		sa->sa_fd = msg->msg_fd;

		log_debug("%s: updated SA peer %s local %s", __func__,
		    print_host(&sa->sa_peer.addr, NULL, 0),
		    print_host(&sa->sa_local.addr, NULL, 0));
	}

	if (ikev2_parse_message(env, hdr, msg, msg->msg_offset) != 0) {
		log_debug("%s: failed to parse message", __func__);
		return;
	}

	if (msg->msg_response)
		return;

	switch (hdr->ike_exchange) {
	case IKEV2_EXCHANGE_IKE_SA_INIT:
		sa = msg->msg_sa;

		if (sa == NULL) {
			log_debug("%s: invalid sa", __func__);
			return;
		}

		sa_state(env, sa, IKEV2_STATE_SA_INIT);

		if ((ibuf_length(sa->sa_inonce) < IKED_NONCE_MIN) ||
		    (ibuf_length(sa->sa_rnonce) < IKED_NONCE_MIN)) {
			log_debug("%s: invalid nonce", __func__);
			sa_free(env, sa);
			return;
		}
		if (ikev2_sa_keys(sa) != 0) {
			log_debug("%s: failed to get IKE SA keys", __func__);
			sa_free(env, sa);
			return;
		}

		if (ikev2_policy2id(&msg->msg_policy->pol_localid,
		    &sa->sa_rid, 1) != 0) {
			log_debug("%s: failed to get responder id", __func__);
			return;
		}
		ibuf_release(sa->sa_1stmsg);
		if ((sa->sa_1stmsg = ibuf_new(ibuf_data(msg->msg_data),
		    ibuf_size(msg->msg_data))) == NULL) {
			log_debug("%s: failed to copy 1st message", __func__);
			return;
		}

		if (ikev2_resp_ike_sa_init(env, msg) != 0) {
			log_debug("%s: failed to send init response", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_IKE_AUTH:
		sa = msg->msg_sa;

		if (!sa_stateok(sa, IKEV2_STATE_AUTH_REQUEST) &&
		    sa->sa_policy->pol_auth.auth_eap)
			sa_state(env, sa, IKEV2_STATE_EAP);

		if (ikev2_resp_ike_auth(env, sa) != 0) {
			log_debug("%s: failed to send auth response", __func__);
			return;
		}
		break;
	case IKEV2_EXCHANGE_CREATE_CHILD_SA:
		(void)ikev2_resp_create_child_sa(env, msg);
		break;
	default:
		break;
	}

	if (msg->msg_sa && msg->msg_sa->sa_state == IKEV2_STATE_DELETE)
		sa_free(env, msg->msg_sa);
}

int
ikev2_message_valid_ike_sa(struct iked *env, struct ike_header *oldhdr,
    struct iked_message *msg)
{
#if 0
	/* XXX Disabled, see comment below */
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ikev2_notify		*n;
	struct ibuf			*buf;
	struct iked_sa			 sa;
#endif

	if (msg->msg_sa != NULL && msg->msg_policy != NULL)
		return (0);

#if 0
	/*
	 * XXX Sending INVALID_IKE_SPIs notifications is disabled
	 * XXX because it is not mandatory and ignored by most
	 * XXX implementations.  We might want to enable it in
	 * XXX combination with a rate-limitation to avoid DoS situations.
	 */

	/* Fail without error message */
	if (msg->msg_response || msg->msg_policy == NULL)
		return (-1);

	/* Invalid IKE SA, return notification */
	if ((buf = ikev2_message_init(env, &resp,
	    &msg->msg_peer, msg->msg_peerlen,
	    &msg->msg_local, msg->msg_locallen, 1)) == NULL)
		goto done;

	bzero(&sa, sizeof(sa));
	if ((oldhdr->ike_flags & IKEV2_FLAG_INITIATOR) == 0)
		sa.sa_hdr.sh_initiator = 1;
	sa.sa_hdr.sh_ispi = betoh64(oldhdr->ike_ispi);
	sa.sa_hdr.sh_rspi = betoh64(oldhdr->ike_rspi);

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, &sa, betoh32(oldhdr->ike_msgid),
	    IKEV2_PAYLOAD_NOTIFY, IKEV2_EXCHANGE_INFORMATIONAL,
	    IKEV2_FLAG_RESPONSE)) == NULL)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
		goto done;
	n->n_protoid = IKEV2_SAPROTO_IKE;
	n->n_spisize = 0;
	n->n_type = htobe16(IKEV2_N_INVALID_IKE_SPI);

	if (ikev2_next_payload(pld, sizeof(*n), IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	(void)ikev2_parse_message(env, hdr, &resp, 0);
	(void)ikev2_message_send(env, msg->msg_fd, &resp);

 done:
	message_cleanup(env, &resp);
#endif

	/* Always fail */
	return (-1);
}

int
ikev2_policy2id(struct iked_static_id *polid, struct iked_id *id, int srcid)
{
	struct ikev2_id		 hdr;
	struct iked_static_id	 localpid;
	char			 idstr[IKED_ID_SIZE];
	struct in_addr		 in4;
	struct in6_addr		 in6;

	/* Fixup the local Id if not specified */
	if (srcid && polid->id_type == 0) {
		polid = &localpid;
		bzero(polid, sizeof(*polid));

		/* Create a default local ID based on our FQDN */
		polid->id_type = IKEV2_ID_FQDN;
		if (gethostname((char *)polid->id_data,
		    sizeof(polid->id_data)) != 0)
			return (-1);
		polid->id_length =
		    strlen((char *)polid->id_data); /* excluding NUL */
	}

	if (!polid->id_length)
		return (-1);

	/* Create an IKEv2 ID payload */
	bzero(&hdr, sizeof(hdr));
	hdr.id_type = id->id_type = polid->id_type;

	if ((id->id_buf = ibuf_new(&hdr, sizeof(hdr))) == NULL)
		return (-1);

	switch (id->id_type) {
	case IKEV2_ID_IPV4_ADDR:
		if (inet_pton(AF_INET, polid->id_data, &in4) != 1 ||
		    ibuf_add(id->id_buf, &in4, sizeof(in4)) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	case IKEV2_ID_IPV6_ADDR:
		if (inet_pton(AF_INET6, polid->id_data, &in6) != 1 ||
		    ibuf_add(id->id_buf, &in6, sizeof(in6)) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	default:
		if (ibuf_add(id->id_buf,
		    polid->id_data, polid->id_length) != 0) {
			ibuf_release(id->id_buf);
			return (-1);
		}
		break;
	}

	if (print_id(id, sizeof(hdr), idstr, sizeof(idstr)) == -1)
		return (-1);

	log_debug("%s: %s %s/%s length %d", __func__,
	    srcid ? "srcid" : "dstid",
	    print_map(id->id_type, ikev2_id_map),
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
ikev2_add_ts(struct ibuf *buf, u_int type, struct iked_sa *sa)
{
	struct iked_policy	*pol = sa->sa_policy;
	struct ikev2_tsp	*tsp;
	struct ikev2_ts		*ts;
	struct iked_flow	*flow;
	struct iked_addr	*addr;
	u_int8_t		*ptr;
	size_t			 len = 0;
	u_int32_t		 av[4], bv[4], mv[4];
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;

	if ((tsp = ibuf_advance(buf, sizeof(*tsp))) == NULL)
		return (-1);
	tsp->tsp_count = pol->pol_nflows;
	len = sizeof(*tsp);

	TAILQ_FOREACH(flow, &pol->pol_flows, flow_entry) {
		if ((ts = ibuf_advance(buf, sizeof(*ts))) == NULL)
			return (-1);

		if (type == IKEV2_PAYLOAD_TSi)
			addr = &flow->flow_src;
		else if (type == IKEV2_PAYLOAD_TSr)
			addr = &flow->flow_dst;
		else
			return (-1);

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

int
ikev2_next_payload(struct ikev2_payload *pld, size_t length,
    u_int8_t nextpayload)
{
	size_t	 pldlength = sizeof(*pld) + length;

	if (pldlength > UINT16_MAX) {
		log_debug("%s: payload too long", __func__);
		return (-1);
	}

	log_debug("%s: length %d nextpayload %s",
	    __func__, pldlength, print_map(nextpayload, ikev2_payload_map));

	pld->pld_length = htobe16(pldlength);
	pld->pld_nextpayload = nextpayload;

	return (0);
}

ssize_t
ikev2_nat_detection(struct iked_message *msg, void *ptr, size_t len,
    u_int type, int response)
{
	EVP_MD_CTX		 ctx;
	struct ike_header	*hdr;
	u_int8_t		 md[SHA_DIGEST_LENGTH];
	u_int			 mdlen = sizeof(md);
	struct iked_sa		*sa = msg->msg_sa;
	struct sockaddr_in	*in4;
	struct sockaddr_in6	*in6;
	ssize_t			 ret = -1;
	struct sockaddr_storage	*ss;
	u_int64_t		 rspi, ispi;

	if (ptr == NULL)
		return (mdlen);

	if (!response || sa == NULL) {
		if ((hdr = ibuf_seek(msg->msg_data, 0, sizeof(*hdr))) == NULL)
			return (-1);
		ispi = hdr->ike_ispi;
		rspi = hdr->ike_rspi;
	} else {
		ispi = htobe64(sa->sa_hdr.sh_ispi);
		rspi = htobe64(sa->sa_hdr.sh_rspi);
	}

	EVP_MD_CTX_init(&ctx);
	EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);

	switch (type) {
	case IKEV2_N_NAT_DETECTION_SOURCE_IP:
		if (response)
			ss = &msg->msg_local;
		else
			ss = &msg->msg_peer;

		log_debug("%s: source %s %s %s", __func__,
		    print_spi(betoh64(ispi), 8),
		    print_spi(betoh64(rspi), 8),
		    print_host(ss, NULL, 0));
		break;
	case IKEV2_N_NAT_DETECTION_DESTINATION_IP:
		if (response)
			ss = &msg->msg_peer;
		else
			ss = &msg->msg_local;

		log_debug("%s: destination %s %s %s", __func__,
		    print_spi(betoh64(ispi), 8),
		    print_spi(betoh64(rspi), 8),
		    print_host(ss, NULL, 0));
		break;
	default:
		goto done;
	}

	EVP_DigestUpdate(&ctx, &ispi, sizeof(ispi));
	EVP_DigestUpdate(&ctx, &rspi, sizeof(rspi));

	switch (ss->ss_family) {
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
			in4 = (struct sockaddr_in *)&ikecfg->cfg.address.addr;
			cfg->cfg_length = htobe16(4);
			if (ibuf_add(buf, &in4->sin_addr.s_addr, 4) == -1)
				return (-1);
			len += 4;
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
			/* 16 bytes IPv6 address + 1 byte prefix length */
			in6 = (struct sockaddr_in6 *)&ikecfg->cfg.address.addr;
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
ikev2_add_proposals(struct ibuf *buf, struct iked_proposals *proposals,
    u_int8_t protoid)
{
	struct ikev2_sa_proposal	*sap;
	struct iked_transform		*xform;
	struct iked_proposal		*prop;
	ssize_t				 i, length = 0, saplength, ret, n;
	u_int64_t			 spi64;
	u_int32_t			 spi32;

	n = 0;
	TAILQ_FOREACH(prop, proposals, prop_entry) {
		if (prop->prop_protoid != protoid)
			continue;

		if ((sap = ibuf_advance(buf, sizeof(*sap))) == NULL) {
			log_debug("%s: failed to add proposal", __func__);
			return (-1);
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

			if ((ret = ikev2_add_transform(buf,
			    i == prop->prop_nxforms - 1 ?
			    IKEV2_XFORM_LAST : IKEV2_XFORM_MORE,
			    xform->xform_type, xform->xform_id,
			    xform->xform_length)) == -1)
				return (-1);

			saplength += ret;
		}

		sap->sap_length = htobe16(saplength);
		length += saplength;
	}

	log_debug("%s: length %d", __func__, length);

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

struct ibuf *
ikev2_message_init(struct iked *env, struct iked_message *msg,
    struct sockaddr_storage *peer, socklen_t peerlen,
    struct sockaddr_storage *local, socklen_t locallen, int response)
{
	bzero(msg, sizeof(*msg));
	memcpy(&msg->msg_peer, peer, peerlen);
	msg->msg_peerlen = peerlen;
	memcpy(&msg->msg_local, local, locallen);
	msg->msg_locallen = locallen;
	msg->msg_response = response ? 1 : 0;
	msg->msg_fd = -1;
	msg->msg_data = ibuf_static();

	return (msg->msg_data);
}

int
ikev2_message_send(struct iked *env, int fd, struct iked_message *msg)
{
	struct ibuf	*buf = msg->msg_data;
	u_int32_t	 natt = 0x00000000;
	struct ike_header	*hdr;

	if (buf == NULL || (hdr = ibuf_seek(msg->msg_data,
	    msg->msg_offset, sizeof(*hdr))) == NULL)
		return (-1);

	log_info("%s: %s to %s from %s", __func__,
	    print_map(hdr->ike_exchange, ikev2_exchange_map),
	    print_host(&msg->msg_peer, NULL, 0),
	    print_host(&msg->msg_local, NULL, 0));

	if (msg->msg_natt || (msg->msg_sa && msg->msg_sa->sa_natt)) {
		if (ibuf_prepend(buf, &natt, sizeof(natt)) == -1) {
			log_debug("%s: failed to set NAT-T", __func__);
			return (-1);
		}
	}
	if ((sendto(fd, ibuf_data(buf), ibuf_size(buf), 0,
	    (struct sockaddr *)&msg->msg_peer, msg->msg_peerlen)) == -1) {
		log_warn("%s: sendto", __func__);
		return (-1);
	}

	return (0);
}

u_int32_t
ikev2_message_id(struct iked *env, struct iked_sa *sa, int response)
{
	if (response)
		return (sa->sa_msgid);

	if (++sa->sa_msgid == UINT32_MAX) {
		/* XXX we should close and renegotiate the connection now */
		log_debug("%s: IKEv2 message sequence overflow", __func__);
	}

	return (sa->sa_msgid);
}

struct ibuf *
ikev2_message_encrypt(struct iked *env, struct iked_sa *sa, struct ibuf *src)
{
	size_t			 len, ivlen, encrlen, integrlen, blocklen,
				    outlen;
	u_int8_t		*buf, pad = 0, *ptr;
	struct ibuf		*integr, *encr, *dst = NULL, *out = NULL;

	buf = ibuf_data(src);
	len = ibuf_size(src);

	log_debug("%s: decrypted length %d", __func__, len);
	print_hex(buf, 0, len);

	if (sa == NULL ||
	    sa->sa_encr == NULL ||
	    sa->sa_integr == NULL) {
		log_debug("%s: invalid SA", __func__);
		goto done;
	}

	if (sa->sa_hdr.sh_initiator) {
		encr = sa->sa_key_iencr;
		integr = sa->sa_key_iauth;
	} else {
		encr = sa->sa_key_rencr;
		integr = sa->sa_key_rauth;
	}

	blocklen = cipher_length(sa->sa_encr);
	ivlen = cipher_ivlength(sa->sa_encr);
	integrlen = hash_length(sa->sa_integr);
	encrlen = roundup(len + sizeof(pad), blocklen);
	pad = encrlen - (len + sizeof(pad));

	/*
	 * Pad the payload and encrypt it
	 */
	if (pad) {
		if ((ptr = ibuf_advance(src, pad)) == NULL)
			goto done;
		arc4random_buf(ptr, pad);
	}
	if (ibuf_add(src, &pad, sizeof(pad)) != 0)
		goto done;

	log_debug("%s: padded length %d", __func__, ibuf_size(src));
	print_hex(ibuf_data(src), 0, ibuf_size(src));

	cipher_setkey(sa->sa_encr, encr->buf, ibuf_length(encr));
	cipher_setiv(sa->sa_encr, NULL, 0);	/* new IV */
	cipher_init_encrypt(sa->sa_encr);

	if ((dst = ibuf_dup(sa->sa_encr->encr_iv)) == NULL)
		goto done;

	if ((out = ibuf_new(NULL,
	    cipher_outlength(sa->sa_encr, encrlen))) == NULL)
		goto done;

	outlen = ibuf_size(out);
	cipher_update(sa->sa_encr,
	    ibuf_data(src), encrlen, ibuf_data(out), &outlen);

	if (outlen && ibuf_add(dst, ibuf_data(out), outlen) != 0)
		goto done;

	outlen = cipher_outlength(sa->sa_encr, 0);
	cipher_final(sa->sa_encr, out->buf, &outlen);
	if (outlen)
		ibuf_add(dst, out->buf, outlen);

	if ((ptr = ibuf_advance(dst, integrlen)) == NULL)
		goto done;
	bzero(ptr, integrlen);

	log_debug("%s: length %d, padding %d, output length %d",
	    __func__, len + sizeof(pad), pad, ibuf_size(dst));
	print_hex(ibuf_data(dst), 0, ibuf_size(dst));

	ibuf_release(src);
	ibuf_release(out);
	return (dst);
 done:
	ibuf_release(src);
	ibuf_release(out);
	ibuf_release(dst);
	return (NULL);
}

int
ikev2_message_integr(struct iked *env, struct iked_sa *sa, struct ibuf *src)
{
	int			 ret = -1;
	size_t			 integrlen, tmplen;
	struct ibuf		*integr, *prf, *tmp = NULL;
	u_int8_t		*ptr;

	log_debug("%s: message length %d", __func__, ibuf_size(src));
	print_hex(ibuf_data(src), 0, ibuf_size(src));

	if (sa == NULL ||
	    sa->sa_integr == NULL) {
		log_debug("%s: invalid SA", __func__);
		return (-1);
	}

	if (sa->sa_hdr.sh_initiator) {
		integr = sa->sa_key_iauth;
		prf = sa->sa_key_iprf;
	} else {
		integr = sa->sa_key_rauth;
		prf = sa->sa_key_rprf;
	}

	integrlen = hash_length(sa->sa_integr);

	log_debug("%s: integrity checksum length %d", __func__,
	    integrlen);

	/*
	 * Validate packet checksum
	 */
	if ((tmp = ibuf_new(NULL, hash_keylength(sa->sa_integr))) == NULL)
		goto done;

	hash_setkey(sa->sa_integr, ibuf_data(integr), ibuf_size(integr));
	hash_init(sa->sa_integr);
	hash_update(sa->sa_integr, ibuf_data(src),
	    ibuf_size(src) - integrlen);
	hash_final(sa->sa_integr, ibuf_data(tmp), &tmplen);

	if (tmplen != integrlen) {
		log_debug("%s: hash failure", __func__);
		goto done;
	}

	if ((ptr = ibuf_seek(src,
	    ibuf_size(src) - integrlen, integrlen)) == NULL)
		goto done;
	memcpy(ptr, ibuf_data(tmp), tmplen);

	print_hex(ibuf_data(tmp), 0, ibuf_size(tmp));

	ret = 0;
 done:
	ibuf_release(tmp);

	return (ret);
}

struct ibuf *
ikev2_message_decrypt(struct iked *env, struct iked_sa *sa,
    struct ibuf *msg, struct ibuf *src)
{
	size_t			 ivlen, encrlen, integrlen, blocklen,
				    outlen, tmplen;
	u_int8_t		 pad, *ptr;
	struct ibuf		*integr, *encr, *tmp = NULL, *out = NULL;
	off_t			 ivoff, encroff, integroff;

	if (sa == NULL ||
	    sa->sa_encr == NULL ||
	    sa->sa_integr == NULL) {
		log_debug("%s: invalid SA", __func__);
		print_hex(ibuf_data(src), 0, ibuf_size(src));
		goto done;
	}

	if (!sa->sa_hdr.sh_initiator) {
		encr = sa->sa_key_iencr;
		integr = sa->sa_key_iauth;
	} else {
		encr = sa->sa_key_rencr;
		integr = sa->sa_key_rauth;
	}

	blocklen = cipher_length(sa->sa_encr);
	ivlen = cipher_ivlength(sa->sa_encr);
	ivoff = 0;
	integrlen = hash_length(sa->sa_integr);
	integroff = ibuf_size(src) - integrlen;
	encroff = ivlen;
	encrlen = ibuf_size(src) - integrlen - ivlen;

	log_debug("%s: IV length %d", __func__, ivlen);
	print_hex(ibuf_data(src), 0, ivlen);
	log_debug("%s: encrypted payload length %d", __func__, encrlen);
	print_hex(ibuf_data(src), encroff, encrlen);
	log_debug("%s: integrity checksum length %d", __func__, integrlen);
	print_hex(ibuf_data(src), integroff, integrlen);

	/*
	 * Validate packet checksum
	 */
	if ((tmp = ibuf_new(NULL, ibuf_length(integr))) == NULL)
		goto done;

	hash_setkey(sa->sa_integr, integr->buf, ibuf_length(integr));
	hash_init(sa->sa_integr);
	hash_update(sa->sa_integr, ibuf_data(msg),
	    ibuf_size(msg) - integrlen);
	hash_final(sa->sa_integr, tmp->buf, &tmplen);

	if (memcmp(tmp->buf, ibuf_data(src) + integroff, integrlen) != 0) {
		log_debug("%s: integrity check failed", __func__);
		goto done;
	}

	log_debug("%s: integrity check succeeded", __func__, tmplen);
	print_hex(tmp->buf, 0, tmplen);

	ibuf_release(tmp);
	tmp = NULL;

	/*
	 * Decrypt the payload and strip any padding
	 */
	if ((encrlen % blocklen) != 0) {
		log_debug("%s: unaligned encrypted payload", __func__);
		goto done;
	}

	cipher_setkey(sa->sa_encr, encr->buf, ibuf_length(encr));
	cipher_setiv(sa->sa_encr, ibuf_data(src) + ivoff, ivlen);
	cipher_init_decrypt(sa->sa_encr);

	if ((out = ibuf_new(NULL, cipher_outlength(sa->sa_encr,
	    encrlen))) == NULL)
		goto done;

	outlen = ibuf_length(out);
	/* XXX why does it need encrlen + blocklen to work correctly? */
	cipher_update(sa->sa_encr,
	    ibuf_data(src) + encroff, encrlen + blocklen,
	    ibuf_data(out), &outlen);
	cipher_final(sa->sa_encr, ibuf_seek(out, outlen, blocklen), &tmplen);
	if (tmplen)
		outlen += tmplen;

	/*
	 * XXX 
	 * XXX the padding is wrong
	 * XXX
	 */
	ptr = ibuf_seek(out, outlen - 1, 1);
	pad = *ptr;

	log_debug("%s: decrypted payload length %d/%d padding %d",
	    __func__, outlen, encrlen, pad);
	print_hex(ibuf_data(out), 0, ibuf_size(out));

	if (ibuf_setsize(out, outlen) != 0)
		goto done;

	ibuf_release(src);
	return (out);
 done:
	ibuf_release(tmp);
	ibuf_release(out);
	ibuf_release(src);
	return (NULL);
}

int
ikev2_resp_ike_sa_init(struct iked *env, struct iked_message *msg)
{
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ikev2_cert		*cert;
	struct ikev2_keyexchange	*ke;
	struct ikev2_notify		*n;
	struct iked_sa			*sa = msg->msg_sa;
	struct ibuf			*buf;
	struct group			*group;
	u_int8_t			*ptr;
	ssize_t				 len;
	int				 ret = -1;

	if ((buf = ikev2_message_init(env, &resp,
	    &msg->msg_peer, msg->msg_peerlen,
	    &msg->msg_local, msg->msg_locallen, 1)) == NULL)
		goto done;

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, sa, 0,
	    IKEV2_PAYLOAD_SA, IKEV2_EXCHANGE_IKE_SA_INIT,
	    IKEV2_FLAG_RESPONSE)) == NULL)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(buf, &sa->sa_proposals,
	    IKEV2_SAPROTO_IKE)) == -1)
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
		len = ikev2_nat_detection(msg, NULL, 0, 0, 1);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(msg, ptr, len,
		    betoh16(n->n_type), 1)) == -1)
			goto done;
		len += sizeof(*n);

		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NOTIFY) == -1)
			goto done;

		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((n = ibuf_advance(buf, sizeof(*n))) == NULL)
			goto done;
		n->n_type = htobe16(IKEV2_N_NAT_DETECTION_DESTINATION_IP);
		len = ikev2_nat_detection(msg, NULL, 0, 0, 1);
		if ((ptr = ibuf_advance(buf, len)) == NULL)
			goto done;
		if ((len = ikev2_nat_detection(msg, ptr, len,
		    betoh16(n->n_type), 1)) == -1)
			goto done;
		len += sizeof(*n);
	}

	if (env->sc_certreqtype) {
		if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_CERTREQ) == -1)
			goto done;

		/* CERTREQ payload */
		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;
		if ((cert = ibuf_advance(buf, sizeof(*cert))) == NULL)
			goto done;
		cert->cert_type = env->sc_certreqtype;
		if (ikev2_add_buf(buf, env->sc_certreq) == -1)
			goto done;
		len = ibuf_size(env->sc_certreq) + sizeof(*cert);
	}

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	(void)ikev2_parse_message(env, hdr, &resp, 0);

	ibuf_release(sa->sa_2ndmsg);
	if ((sa->sa_2ndmsg = ibuf_dup(buf)) == NULL) {
		log_debug("%s: failed to copy 2nd message", __func__);
		goto done;
	}

	ret = ikev2_message_send(env, msg->msg_fd, &resp);

 done:
	message_cleanup(env, &resp);

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

	if (ikev2_childsa_negotiate(env, sa, NULL) == -1)
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
		if ((sa->sa_staterequire & IKED_REQ_CERT) &&
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

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_SA) == -1)
		goto done;

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_proposals(e, &sa->sa_proposals,
	    IKEV2_SAPROTO_ESP)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_TSi) == -1)
		goto done;

	/* TSi payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_ts(e, IKEV2_PAYLOAD_TSi, sa)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_TSr) == -1)
		goto done;

	/* TSr payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_ts(e, IKEV2_PAYLOAD_TSr, sa)) == -1)
		goto done;

 send:
	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_message_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_IKE_AUTH, firstpayload, 1);
	if (ret == 0)
		ret = ikev2_childsa_enable(env, sa);
	if (ret == 0)
		sa_state(env, sa, IKEV2_STATE_RUNNING);

 done:
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

	if (!sa_stateok(sa, IKEV2_STATE_EAP))
		return (0);

	/* Responder only */
	if (sa->sa_hdr.sh_initiator)
		return (-1);

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

	if ((sa->sa_staterequire & IKED_REQ_CERT) &&
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

	ret = ikev2_message_send_encrypt(env, sa, &e,
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
	if (ibuf_cat(e, buf) != 0)
		goto done;

	if (ikev2_next_payload(pld, ibuf_size(buf), IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_message_send_encrypt(env, sa, &e,
	    exchange, firstpayload, response);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_resp_create_child_sa(struct iked *env, struct iked_message *msg)
{
	struct ikev2_payload		*pld;
	struct ibuf			*e = NULL, *nonce = NULL;
	u_int				 firstpayload;
	int				 ret = -1;
	ssize_t				 len = 0;
	struct iked_sa			*sa = msg->msg_sa;
	struct iked_spi			*rekey = &msg->msg_rekey;

	if (msg->msg_response)
		return (0);

	log_debug("%s: rekey %s spi %s", __func__,
	    print_map(rekey->spi_protoid, ikev2_saproto_map),
	    print_spi(rekey->spi, rekey->spi_size));

	if (rekey->spi_protoid == 0) {
		/* Default to IKE_SA if REKEY_SA was not notified */
		rekey->spi_protoid = IKEV2_SAPROTO_IKE;
		if (sa->sa_hdr.sh_initiator)
			rekey->spi = sa->sa_hdr.sh_rspi;
		else
			rekey->spi = sa->sa_hdr.sh_ispi;
		rekey->spi_size = 8;
	}

	if (ikev2_childsa_negotiate(env, sa, rekey) == -1)
		return (-1);

	if (sa->sa_hdr.sh_initiator)
		nonce = sa->sa_inonce;
	else
		nonce = sa->sa_rnonce;

	if ((e = ibuf_static()) == NULL)
		return (-1);

	/* SA payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	firstpayload = IKEV2_PAYLOAD_SA;
	if ((len = ikev2_add_proposals(e, &sa->sa_proposals,
	    IKEV2_SAPROTO_ESP)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONCE) == -1)
		goto done;

	/* NONCE payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if (ikev2_add_buf(e, nonce) == -1)
		goto done;
	len = ibuf_size(nonce);

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_TSi) == -1)
		goto done;

	/* TSi payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_ts(e, IKEV2_PAYLOAD_TSi, sa)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_TSr) == -1)
		goto done;

	/* TSr payload */
	if ((pld = ikev2_add_payload(e)) == NULL)
		goto done;
	if ((len = ikev2_add_ts(e, IKEV2_PAYLOAD_TSr, sa)) == -1)
		goto done;

	if (ikev2_next_payload(pld, len, IKEV2_PAYLOAD_NONE) == -1)
		goto done;

	ret = ikev2_message_send_encrypt(env, sa, &e,
	    IKEV2_EXCHANGE_CREATE_CHILD_SA, firstpayload, 1);
	if (ret == 0)
		ret = ikev2_childsa_enable(env, sa);

 done:
	ibuf_release(e);
	return (ret);
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

	if ((buf = ikev2_message_init(env, &resp,
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

	if (sa != NULL && msg->msg_decrypted) {
		/* IKE header */
		if ((hdr = ikev2_add_header(buf, sa,
		    ikev2_message_id(env, sa, 0),
		    IKEV2_PAYLOAD_E, IKEV2_EXCHANGE_INFORMATIONAL,
		    0)) == NULL)
			goto done;

		if ((pld = ikev2_add_payload(buf)) == NULL)
			goto done;

		/* Encrypt message and add as an E payload */
		if ((e = ikev2_message_encrypt(env, sa, e)) == NULL) {
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
		if (ikev2_message_integr(env, sa, buf) != 0) {
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
		sa = &sah;

		/* IKE header */
		if ((hdr = ikev2_add_header(buf, &sah,
		    ikev2_message_id(env, &sah, 0),
		    IKEV2_PAYLOAD_NOTIFY, IKEV2_EXCHANGE_INFORMATIONAL,
		    0)) == NULL)
			goto done;
		if (ibuf_cat(buf, e) != 0)
			goto done;
		if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
			goto done;
	}

	resp.msg_data = buf;
	resp.msg_sa = sa;
	TAILQ_INIT(&resp.msg_proposals);

	sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;
	(void)ikev2_parse_message(env, hdr, &resp, 0);
	sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;

	ret = ikev2_message_send(env, msg->msg_fd, &resp);

 done:
	ibuf_release(e);
	message_cleanup(env, &resp);

	return (ret);
}

int
ikev2_message_send_encrypt(struct iked *env, struct iked_sa *sa,
    struct ibuf **ep, u_int8_t exchange, u_int8_t firstpayload, int response)
{
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ibuf			*buf, *e = *ep;
	int				 ret = -1;

	if ((buf = ikev2_message_init(env, &resp,
	    &sa->sa_peer.addr, sa->sa_peer.addr.ss_len,
	    &sa->sa_local.addr, sa->sa_local.addr.ss_len, 1)) == NULL)
		goto done;

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, sa,
	    ikev2_message_id(env, sa, response),
	    IKEV2_PAYLOAD_E, exchange,
	    response ? IKEV2_FLAG_RESPONSE : 0)) == NULL)
		goto done;

	if ((pld = ikev2_add_payload(buf)) == NULL)
		goto done;

	/* Encrypt message and add as an E payload */
	if ((e = ikev2_message_encrypt(env, sa, e)) == NULL) {
		log_debug("%s: encryption failed", __func__);
		goto done;
	}
	if (ibuf_cat(buf, e) != 0)
		goto done;
	if (ikev2_next_payload(pld, ibuf_size(e), firstpayload) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	/* Add integrity checksum (HMAC) */
	if (ikev2_message_integr(env, sa, buf) != 0) {
		log_debug("%s: integrity checksum failed", __func__);
		goto done;
	}

	resp.msg_data = buf;
	resp.msg_sa = sa;
	TAILQ_INIT(&resp.msg_proposals);

	sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;
	(void)ikev2_parse_message(env, hdr, &resp, 0);
	sa->sa_hdr.sh_initiator = sa->sa_hdr.sh_initiator ? 0 : 1;

	ret = ikev2_message_send(env, sa->sa_fd, &resp);

 done:
	/* e is cleaned up by the calling function */
	*ep = e;
	message_cleanup(env, &resp);

	return (ret);
}

struct ibuf *
ikev2_message_auth(struct iked *env, struct iked_sa *sa, int response)
{
	struct ibuf		*authmsg = NULL, *nonce, *prfkey, *buf;
	u_int8_t		*ptr;
	struct iked_id		*id;
	size_t			 tmplen;

	/*
	 * Create the payload to be signed/MAC'ed for AUTH
	 */

	if (!response) {
		if ((nonce = sa->sa_rnonce) == NULL ||
		    (sa->sa_iid.id_type == 0) ||
		    (prfkey = sa->sa_key_iprf) == NULL ||
		    (buf = sa->sa_1stmsg) == NULL)
			return (NULL);
		id = &sa->sa_iid;
	} else {
		if ((nonce = sa->sa_inonce) == NULL ||
		    (sa->sa_rid.id_type == 0) ||
		    (prfkey = sa->sa_key_rprf) == NULL ||
		    (buf = sa->sa_2ndmsg) == NULL)
			return (NULL);
		id = &sa->sa_rid;
	}

	if ((authmsg = ibuf_dup(buf)) == NULL)
		return (NULL);
	if (ibuf_cat(authmsg, nonce) != 0)
		goto fail;

	if ((hash_setkey(sa->sa_prf, ibuf_data(prfkey),
	    ibuf_size(prfkey))) == NULL)
		goto fail;

	if ((ptr = ibuf_advance(authmsg,
	    hash_length(sa->sa_prf))) == NULL)
		goto fail;

	hash_init(sa->sa_prf);
	hash_update(sa->sa_prf, ibuf_data(id->id_buf), ibuf_size(id->id_buf));
	hash_final(sa->sa_prf, ptr, &tmplen);

	if (tmplen != hash_length(sa->sa_prf))
		goto fail;

	log_debug("%s: %s auth data length %d",
	    __func__, response ? "responder" : "initiator",
	    ibuf_size(authmsg));
	print_hex(ibuf_data(authmsg), 0, ibuf_size(authmsg));

	return (authmsg);

 fail:
	ibuf_release(authmsg);
	return (NULL);
}

int
ikev2_parse_message(struct iked *env, struct ike_header *hdr,
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

	return (ikev2_parse_payloads(env, msg, offset,
	    betoh32(hdr->ike_length), hdr->ike_nextpayload, 0));
}

int
ikev2_parse_payloads(struct iked *env, struct iked_message *msg,
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
		ikev2_parse_payloads(env, msg, offset, length, payload, 1);

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
			ret = ikev2_parse_sa(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_KE:
			ret = ikev2_parse_ke(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_IDi | IKED_E:
		case IKEV2_PAYLOAD_IDr | IKED_E:
			ret = ikev2_parse_id(env, &pld, msg, offset, payload);
			break;
		case IKEV2_PAYLOAD_CERT | IKED_E:
			ret = ikev2_parse_cert(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_CERTREQ:
		case IKEV2_PAYLOAD_CERTREQ | IKED_E:
			ret = ikev2_parse_certreq(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_AUTH | IKED_E:
			ret = ikev2_parse_auth(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_NONCE:
		case IKEV2_PAYLOAD_NONCE | IKED_E:
			ret = ikev2_parse_nonce(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_NOTIFY:
		case IKEV2_PAYLOAD_NOTIFY | IKED_E:
			ret = ikev2_parse_notify(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_DELETE | IKED_E:
			ret = ikev2_parse_delete(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_TSi | IKED_E:
		case IKEV2_PAYLOAD_TSr | IKED_E:
			ret = ikev2_parse_ts(env, &pld, msg, offset, payload);
			break;
		case IKEV2_PAYLOAD_E:
			ret = ikev2_parse_e(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_CP | IKED_E:
			ret = ikev2_parse_cp(env, &pld, msg, offset);
			break;
		case IKEV2_PAYLOAD_EAP | IKED_E:
			ret = ikev2_parse_eap(env, &pld, msg, offset);
			break;
		default:
			print_hex(msgbuf, offset,
			    betoh16(pld.pld_length) - sizeof(pld));
			break;
		}

		if (ret != 0 && !msg->msg_response) {
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
ikev2_parse_sa(struct iked *env, struct ikev2_payload *pld,
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

	if (!msg->msg_response) {
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
	if (ikev2_parse_xform(env, &sap, msg, offset) != 0) {
		log_debug("%s: invalid proposal transforms", __func__);
		return (-1);
	}

	if (msg->msg_response)
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
ikev2_parse_xform(struct iked *env, struct ikev2_sa_proposal *sap,
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
		ikev2_parse_attr(env, &xfrm, msg, offset + sizeof(xfrm),
		    betoh16(xfrm.xfrm_length) - sizeof(xfrm));

	if (!msg->msg_response) {
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
		ikev2_parse_xform(env, sap, msg, offset);

	return (0);
}

int
ikev2_parse_attr(struct iked *env, struct ikev2_transform *xfrm,
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
		ikev2_parse_attr(env, xfrm, msg, offset, total);
	}

	return (0);
}

int
ikev2_parse_ke(struct iked *env, struct ikev2_payload *pld,
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

	if (!msg->msg_response) {
		if ((msg->msg_sa->sa_dhiexchange =
		    ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get exchange", __func__);
			return (-1);
		}
	}

	return (0);
}

int
ikev2_parse_id(struct iked *env, struct ikev2_payload *pld,
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

	if (msg->msg_response) {
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

	if ((authmsg = ikev2_message_auth(env, sa,
	    !sa->sa_hdr.sh_initiator)) == NULL) {
		log_debug("%s: failed to get response auth data", __func__);
		return (-1);
	}

	ca_setauth(env, sa, authmsg, PROC_CERT);

	return (0);
}

int
ikev2_parse_cert(struct iked *env, struct ikev2_payload *pld,
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

	if (msg->msg_response)
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
ikev2_parse_certreq(struct iked *env, struct ikev2_payload *pld,
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

	if (msg->msg_response)
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
ikev2_parse_auth(struct iked *env, struct ikev2_payload *pld,
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

	if (msg->msg_response)
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

	if ((authmsg = ikev2_message_auth(env, sa,
	    sa->sa_hdr.sh_initiator)) == NULL) {
		log_debug("%s: failed to get auth data", __func__);
		return (-1);
	}

	ret = ikev2_message_authverify(env, sa, &ikeauth, buf, len,
	    authmsg);

	ibuf_release(authmsg);
	authmsg = NULL;

	if (ret != 0)
		goto done;

	if (sa->sa_eapmsk != NULL) {
		if ((authmsg = ikev2_message_auth(env, sa,
		    !sa->sa_hdr.sh_initiator)) == NULL) {
			log_debug("%s: failed to get auth data", __func__);
			return (-1);
		}

		/* 2nd AUTH for EAP messages */
		if ((ret = ikev2_message_authsign(env, sa,
		    &ikeauth, authmsg)) != 0)
			goto done;

		sa_state(env, sa, IKEV2_STATE_EAP_VALID);
	}

 done:
	ibuf_release(authmsg);

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
ikev2_message_authverify(struct iked *env, struct iked_sa *sa,
    struct iked_auth *auth, u_int8_t *buf, size_t len, struct ibuf *authmsg)
{
	u_int8_t			*key, *psk = NULL;
	ssize_t				 keylen;
	struct iked_id			*id;
	struct iked_dsa			*dsa = NULL;
	int				 ret = -1;
	u_int8_t			 keytype;

	if (sa->sa_hdr.sh_initiator)
		id = &sa->sa_rcert;
	else
		id = &sa->sa_icert;

	if ((dsa = dsa_verify_new(auth->auth_method, sa->sa_prf)) == NULL) {
		log_debug("%s: invalid auth method", __func__);
		return (-1);
	}

	switch (auth->auth_method) {
	case IKEV2_AUTH_SHARED_KEY_MIC:
		if (!auth->auth_length) {
			log_debug("%s: no pre-shared key found", __func__);
			goto done;
		}
		if ((keylen = ikev2_psk(sa, auth->auth_data,
		    auth->auth_length, &psk)) == -1) {
			log_debug("%s: failed to get PSK", __func__);
			goto done;
		}
		key = psk;
		keytype = 0;
		break;
	default:
		if (id == NULL) {
			log_debug("%s: no cert found", __func__);
			goto done;
		}
		key = ibuf_data(id->id_buf);
		keylen = ibuf_size(id->id_buf);
		keytype = id->id_type;
		break;
	}

	log_debug("%s: method %s keylen %d type %s", __func__,
	    print_map(auth->auth_method, ikev2_auth_map), keylen,
	    print_map(id->id_type, ikev2_cert_map));

	if (dsa_setkey(dsa, key, keylen, keytype) == NULL ||
	    dsa_init(dsa) != 0 ||
	    dsa_update(dsa, ibuf_data(authmsg), ibuf_size(authmsg))) {
		log_debug("%s: failed to compute digital signature", __func__);
		goto done;
	}

	if ((ret = dsa_verify_final(dsa, buf, len)) == 0) {
		log_debug("%s: authentication successful", __func__);
		sa_state(env, sa, IKEV2_STATE_AUTH_SUCCESS);

		if (!sa->sa_policy->pol_auth.auth_eap &&
		    auth->auth_method == IKEV2_AUTH_SHARED_KEY_MIC)
			sa_state(env, sa, IKEV2_STATE_VALID);
	} else {
		log_debug("%s: authentication failed", __func__);
		sa_state(env, sa, IKEV2_STATE_AUTH_REQUEST);
	}

 done:
	if (psk != NULL)
		free(psk);
	dsa_free(dsa);

	return (ret);
}

int
ikev2_message_authsign(struct iked *env, struct iked_sa *sa,
    struct iked_auth *auth, struct ibuf *authmsg)
{
	u_int8_t			*key, *psk = NULL;
	ssize_t				 keylen;
	struct iked_hash		*prf = sa->sa_prf;
	struct iked_id			*id;
	struct iked_dsa			*dsa = NULL;
	struct ibuf			*buf;
	int				 ret = -1;
	u_int8_t			 keytype;

	if (sa->sa_hdr.sh_initiator)
		id = &sa->sa_icert;
	else
		id = &sa->sa_rcert;

	if ((dsa = dsa_sign_new(auth->auth_method, prf)) == NULL) {
		log_debug("%s: invalid auth method", __func__);
		return (-1);
	}

	switch (auth->auth_method) {
	case IKEV2_AUTH_SHARED_KEY_MIC:
		if (!auth->auth_length) {
			log_debug("%s: no pre-shared key found", __func__);
			goto done;
		}
		if ((keylen = ikev2_psk(sa, auth->auth_data,
		    auth->auth_length, &psk)) == -1) {
			log_debug("%s: failed to get PSK", __func__);
			goto done;
		}
		key = psk;
		keytype = 0;
		break;
	default:
		if (id == NULL) {
			log_debug("%s: no cert found", __func__);
			goto done;
		}
		key = ibuf_data(id->id_buf);
		keylen = ibuf_size(id->id_buf);
		keytype = id->id_type;
		break;
	}

	if (dsa_setkey(dsa, key, keylen, keytype) == NULL ||
	    dsa_init(dsa) != 0 ||
	    dsa_update(dsa, ibuf_data(authmsg), ibuf_size(authmsg))) {
		log_debug("%s: failed to compute digital signature", __func__);
		goto done;
	}

	ibuf_release(sa->sa_localauth.id_buf);
	sa->sa_localauth.id_buf = NULL;

	if ((buf = ibuf_new(NULL, dsa_length(dsa))) == NULL) {
		log_debug("%s: failed to get auth buffer", __func__);
		goto done;
	}

	if ((ret = dsa_sign_final(dsa,
	    ibuf_data(buf), ibuf_size(buf))) == -1) {
		log_debug("%s: failed to create auth signature", __func__);
		ibuf_release(buf);
		goto done;
	}

	sa->sa_localauth.id_type = auth->auth_method;
	sa->sa_localauth.id_buf = buf;

	ret = 0;
 done:
	if (psk != NULL)
		free(psk);
	dsa_free(dsa);

	return (ret);
}

int
ikev2_parse_nonce(struct iked *env, struct ikev2_payload *pld,
    struct iked_message *msg, off_t offset)
{
	size_t		 len;
	u_int8_t	*buf;
	u_int8_t	*msgbuf = ibuf_data(msg->msg_data);
	struct iked_sa	*sa = msg->msg_sa;
	struct ibuf	*localnonce, *peernonce;

	buf = msgbuf + offset;
	len = betoh16(pld->pld_length) - sizeof(*pld);
	print_hex(buf, 0, len);

	if (!msg->msg_response) {
		if ((peernonce = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: failed to get peer nonce", __func__);
			return (-1);
		}
		if ((localnonce =
		    ibuf_random(IKED_NONCE_SIZE)) == NULL) {
			log_debug("%s: failed to get local nonce", __func__);
			ibuf_release(peernonce);
			return (-1);
		}

		ibuf_release(sa->sa_inonce);
		ibuf_release(sa->sa_rnonce);

		log_debug("%s: updating nonces", __func__);

		if (sa->sa_hdr.sh_initiator) {
			sa->sa_inonce = localnonce;
			sa->sa_rnonce = peernonce;
		} else {
			sa->sa_inonce = peernonce;
			sa->sa_rnonce = localnonce;
		}
	}

	return (0);
}

int
ikev2_parse_notify(struct iked *env, struct ikev2_payload *pld,
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
ikev2_parse_delete(struct iked *env, struct ikev2_payload *pld,
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
		if (!msg->msg_response &&
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

	if (!msg->msg_response &&
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
		if (msg->msg_response) {
			log_debug("%s: spi %s", __func__, print_spi(spi, sz));
			continue;
		}

		if (ikev2_childsa_delete(env, sa,
		    del->del_protoid, spi, &localspi[i], 0) == -1)
			failed++;
		else
			found++;
	}

	if (msg->msg_response)
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
ikev2_parse_ts(struct iked *env, struct ikev2_payload *pld,
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
ikev2_parse_e(struct iked *env, struct ikev2_payload *pld,
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

	if ((e = ikev2_message_decrypt(env, msg->msg_sa,
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

	ret = ikev2_parse_payloads(env, &emsg, 0, ibuf_size(e),
	    pld->pld_nextpayload, 0);

 done:
	ibuf_release(e);

	return (ret);
}

int
ikev2_parse_cp(struct iked *env, struct ikev2_payload *pld,
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

	if (msg->msg_response)
		return (0);

	if (sa)
		sa->sa_cp = cp.cp_type;

	return (0);
}

int
ikev2_parse_eap(struct iked *env, struct ikev2_payload *pld,
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

int
ikev2_sa_negotiate(struct iked_sa *sa, struct iked_proposals *local,
    struct iked_proposals *peer, u_int8_t protoid)
{
	struct iked_proposal	*ppeer = NULL, *plocal, *prop, values;
	struct iked_transform	*tpeer, *tlocal;
	struct iked_transform	*match[IKEV2_XFORMTYPE_MAX];
	struct iked_transform	*chosen[IKEV2_XFORMTYPE_MAX];
	u_int			 type, i, j, match_score, chosen_score = 0;

	bzero(chosen, sizeof(chosen));
	bzero(&values, sizeof(values));

	TAILQ_FOREACH(plocal, local, prop_entry) {
		if (plocal->prop_protoid != protoid)
			continue;

		TAILQ_FOREACH(ppeer, peer, prop_entry) {
			bzero(match, sizeof(match));
			match_score = 0;

			if (ppeer->prop_protoid != plocal->prop_protoid)
				continue;

			for (i = 0; i < ppeer->prop_nxforms; i++) {
				tpeer = ppeer->prop_xforms + i;
				for (j = 0; j < plocal->prop_nxforms; j++) {
					tlocal = plocal->prop_xforms + j;
					if (!(tpeer->xform_type ==
					    tlocal->xform_type &&
					    tpeer->xform_id ==
					    tlocal->xform_id &&
					    tpeer->xform_length ==
					    tlocal->xform_length))
						continue;
					if (tpeer->xform_type >
					    IKEV2_XFORMTYPE_MAX)
						continue;
					type = tpeer->xform_type;

					if (match[type] == NULL ||
					    tlocal->xform_score <
					    match[type]->xform_score) {
						match[type] = tlocal;
					} else
						continue;

					print_debug("%s: xform %d "
					    "<-> %d (%d): %s %s (keylength %d <-> %d)", __func__,
					    ppeer->prop_id, plocal->prop_id,
					    tlocal->xform_score,
					    print_map(type,
					    ikev2_xformtype_map),
					    print_map(tpeer->xform_id,
					    tpeer->xform_map),
					    tpeer->xform_keylength,
					    tlocal->xform_keylength);
					if (tpeer->xform_length)
						print_debug(" %d",
						    tpeer->xform_length);
					print_debug("\n");
				}
			}

			for (i = match_score = 0;
			    i < IKEV2_XFORMTYPE_MAX; i++) {
				if (protoid == IKEV2_SAPROTO_IKE &&
				    (match[i] == NULL) && (
				    (i == IKEV2_XFORMTYPE_ENCR) ||
				    (i == IKEV2_XFORMTYPE_PRF) ||
				    (i == IKEV2_XFORMTYPE_INTEGR) ||
				    (i == IKEV2_XFORMTYPE_DH))) {
					match_score = 0;
					break;
				} else if (protoid != IKEV2_SAPROTO_IKE &&
				    (match[i] == NULL) && (
				    (i == IKEV2_XFORMTYPE_ENCR) ||
				    (i == IKEV2_XFORMTYPE_INTEGR) ||
				    (i == IKEV2_XFORMTYPE_ESN))) {
					match_score = 0;
					break;
				} else if (match[i] == NULL)
					continue;

				match_score += match[i]->xform_score;
			}

			log_debug("%s: score %d", __func__, match_score);
			if (match_score != 0 &&
			    (chosen_score == 0 || match_score < chosen_score)) {
				chosen_score = match_score;
				for (i = 0; i < IKEV2_XFORMTYPE_MAX; i++)
					chosen[i] = match[i];
				memcpy(&values, ppeer, sizeof(values));
			}
		}
		if (chosen_score != 0)
			break;
	}

	if (chosen_score == 0)
		return (-1);
	else if (sa == NULL)
		return (0);

	(void)config_free_proposals(&sa->sa_proposals, protoid);
	prop = config_add_proposal(&sa->sa_proposals, values.prop_id, protoid);

	if (values.prop_localspi.spi_size) {
		prop->prop_peerspi = values.prop_peerspi;
		prop->prop_localspi.spi_size = values.prop_localspi.spi_size;
		prop->prop_localspi.spi = 0;
	}

	for (i = 0; i < IKEV2_XFORMTYPE_MAX; i++) {
		if (chosen[i] == NULL)
			continue;
		print_debug("%s: score %d: %s %s",
		    __func__, chosen[i]->xform_score,
		    print_map(i, ikev2_xformtype_map),
		    print_map(chosen[i]->xform_id, chosen[i]->xform_map));
		if (chosen[i]->xform_length)
			print_debug(" %d", chosen[i]->xform_length);
		print_debug("\n");

		if (config_add_transform(prop, chosen[i]->xform_type,
		    chosen[i]->xform_id, chosen[i]->xform_length,
		    chosen[i]->xform_keylength) == NULL)
			break;
	}

	return (0);
}

int
ikev2_sa_keys(struct iked_sa *sa)
{
	struct iked_transform	*xform;
	struct iked_hash	*prf, *integr;
	struct iked_cipher	*encr;
	struct group		*group;
	struct ibuf		*ninr, *dhsecret, *skeyseed, *s, *t;
	size_t			 nonceminlen, ilen, rlen, tmplen;
	u_int64_t		 ispi, rspi;
	int			 ret = -1;

	ninr = dhsecret = skeyseed = s = t = NULL;

	if (sa->sa_encr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_ENCR)) == NULL) {
			log_debug("%s: did not find encr transform", __func__);
			return (-1);
		}
		if ((sa->sa_encr = cipher_new(xform->xform_type,
		    xform->xform_id, xform->xform_length)) == NULL) {
			log_debug("%s: failed to get encr", __func__);
			return (-1);
		}
	}
	encr = sa->sa_encr;

	if (sa->sa_prf == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_PRF)) == NULL) {
			log_debug("%s: did not find prf transform", __func__);
			return (-1);
		}
		if ((sa->sa_prf =
		    hash_new(xform->xform_type, xform->xform_id)) == NULL) {
			log_debug("%s: failed to get prf", __func__);
			return (-1);
		}
	}
	prf = sa->sa_prf;

	if (sa->sa_integr == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_INTEGR)) == NULL) {
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
	integr = sa->sa_integr;

	if (sa->sa_dhgroup == NULL) {
		if ((xform = config_findtransform(&sa->sa_proposals,
		    IKEV2_XFORMTYPE_DH)) == NULL) {
			log_debug("%s: did not find dh transform", __func__);
			return (-1);
		}
		if ((sa->sa_dhgroup =
		    group_get(xform->xform_id)) == NULL) {
			log_debug("%s: invalid dh", __func__);
			return (-1);
		}
		if ((ssize_t)ibuf_length(sa->sa_dhiexchange) !=
		    dh_getlen(sa->sa_dhgroup)) {
			/* XXX send notification to peer */
			log_debug("%s: dh mismatch %d", __func__,
			    dh_getlen(sa->sa_dhgroup) * 8);
			return (-1);
		}
	}
	group = sa->sa_dhgroup;

	if (!ibuf_length(sa->sa_dhrexchange)) {
		if ((sa->sa_dhrexchange = ibuf_new(NULL,
		    dh_getlen(group))) == NULL) {
			log_debug("%s: failed to alloc dh exchange", __func__);
			return (-1);
		}
		if (dh_create_exchange(group, sa->sa_dhrexchange->buf) == -1) {
			log_debug("%s: failed to get dh exchange", __func__);
			return (-1);
		}
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

	/*
	 * First generate SKEEYSEED = prf(Ni | Nr, g^ir)
	 */
	if (prf->hash_fixedkey) {
		/* Half of the key bits must come from Ni, and half from Nr */
		ilen = prf->hash_fixedkey / 2;
		rlen = prf->hash_fixedkey / 2;
	} else {
		/* Most PRF functions accept a variable-length key */
		ilen = ibuf_length(sa->sa_inonce);
		rlen = ibuf_length(sa->sa_rnonce);
	}

	if ((ninr = ibuf_new(sa->sa_inonce->buf, ilen)) == NULL ||
	    ibuf_add(ninr, sa->sa_rnonce->buf, rlen) != 0) {
		log_debug("%s: failed to get nonce key buffer", __func__);
		goto done;
	}
	if ((hash_setkey(prf, ninr->buf, ibuf_length(ninr))) == NULL) {
		log_debug("%s: failed to set prf key", __func__);
		goto done;
	}

	if ((dhsecret = ibuf_new(NULL, dh_getlen(group))) == NULL) {
		log_debug("%s: failed to alloc dh secret", __func__);
		goto done;
	}
	if (dh_create_shared(group, dhsecret->buf,
	    sa->sa_dhiexchange->buf) == -1) {
		log_debug("%s: failed to get dh secret"
		    " group %d len %d secret %d exchange %d", __func__,
		    group->id, dh_getlen(group), ibuf_length(dhsecret),
		    ibuf_length(sa->sa_dhiexchange));
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

	log_debug("%s: SKEYSEED with %d bytes", __func__, tmplen);
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

	log_debug("%s: S with %d bytes", __func__, ibuf_length(s));
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

	if ((sa->sa_key_d = ibuf_copy(t, hash_length(prf))) == NULL ||
	    (sa->sa_key_iauth = ibuf_copy(t, hash_keylength(integr))) == NULL ||
	    (sa->sa_key_rauth = ibuf_copy(t, hash_keylength(integr))) == NULL ||
	    (sa->sa_key_iencr = ibuf_copy(t, cipher_keylength(encr))) == NULL ||
	    (sa->sa_key_rencr = ibuf_copy(t, cipher_keylength(encr))) == NULL ||
	    (sa->sa_key_iprf = ibuf_copy(t, hash_length(prf))) == NULL ||
	    (sa->sa_key_rprf = ibuf_copy(t, hash_length(prf))) == NULL) {
		log_debug("%s: failed to get SA keys", __func__);
		goto done;
	}

	log_debug("%s: SK_d with %d bytes", __func__,
	    ibuf_length(sa->sa_key_d));
	print_hex(sa->sa_key_d->buf, 0, ibuf_length(sa->sa_key_d));
	log_debug("%s: SK_ai with %d bytes", __func__,
	    ibuf_length(sa->sa_key_iauth));
	print_hex(sa->sa_key_iauth->buf, 0, ibuf_length(sa->sa_key_iauth));
	log_debug("%s: SK_ar with %d bytes", __func__,
	    ibuf_length(sa->sa_key_rauth));
	print_hex(sa->sa_key_rauth->buf, 0, ibuf_length(sa->sa_key_rauth));
	log_debug("%s: SK_ei with %d bytes", __func__,
	    ibuf_length(sa->sa_key_iencr));
	print_hex(sa->sa_key_iencr->buf, 0, ibuf_length(sa->sa_key_iencr));
	log_debug("%s: SK_er with %d bytes", __func__,
	    ibuf_length(sa->sa_key_rencr));
	print_hex(sa->sa_key_rencr->buf, 0, ibuf_length(sa->sa_key_rencr));
	log_debug("%s: SK_pi with %d bytes", __func__,
	    ibuf_length(sa->sa_key_iprf));
	print_hex(sa->sa_key_iprf->buf, 0, ibuf_length(sa->sa_key_iprf));
	log_debug("%s: SK_pr with %d bytes", __func__,
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

		log_debug("%s: T%d with %d bytes", __func__,
		    pad, ibuf_length(t1));
		print_hex(t1->buf, 0, ibuf_length(t1));
	}

	log_debug("%s: Tn with %d bytes", __func__, ibuf_length(t));
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

	if (print_id(id, sizeof(struct ikev2_id),
	    idstr, sizeof(idstr)) == -1) {
		log_debug("%s: invalid id", __func__);
		goto fail;
	}

	/* ASN.1 DER IDs are too long, use the CN part instead */
	if (*idstr == '/' && (idrepl = strstr(idstr, "CN=")) != NULL) {
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
		else if (id->id_type == IKEV2_ID_RFC822_ADDR)
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

	log_debug("%s: %s (%d)", __func__, sa->sa_tag, strlen(sa->sa_tag));

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
    struct iked_spi *rekey)
{
	struct iked_proposal	*prop;
	struct iked_transform	*xform, *encrxf = NULL, *integrxf = NULL;
	struct iked_childsa	*csa, *csb;
	struct iked_flow	*flow, *flowa, *flowb;
	struct ibuf		*keymat = NULL, *seed = NULL;
	u_int			 i, mdlen;
	size_t			 ilen = 0;
	int			 ret = -1;
	u_int32_t		 spi = 0, flowloaded = 0;
	struct iked_id		*peerid, *localid;
	EVP_MD_CTX		 ctx;
	u_int8_t		 md[SHA_DIGEST_LENGTH];

	if (!sa_stateok(sa, IKEV2_STATE_VALID))
		return (-1);

	if (sa->sa_hdr.sh_initiator) {
		peerid = &sa->sa_rid;
		localid = &sa->sa_iid;
	} else {
		localid = &sa->sa_rid;
		peerid = &sa->sa_iid;
	}

	if (ikev2_sa_tag(sa, peerid) == -1)
		return (-1);

	sa_stateflags(sa, IKED_REQ_CHILDSA);

	/* We need to determinate the key material length first */
	TAILQ_FOREACH(prop, &sa->sa_proposals, prop_entry) {
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
				ilen += xform->xform_keylength / 8;
				break;
			}
		}
	}

	/* double key material length for inbound/outbound */
	ilen *= 2;

	log_debug("%s: key material length %d", __func__, ilen);

	if ((seed = ibuf_dup(sa->sa_inonce)) == NULL ||
	    ibuf_cat(seed, sa->sa_rnonce) != 0 ||
	    (keymat = ikev2_prfplus(sa->sa_prf,
	    sa->sa_key_d, seed, ilen)) == NULL) {
		log_debug("%s: failed to get IKE SA key material", __func__);
		goto done;
	}

	/*
	 * Generate a hash of the negotiated flows to detect a possible
	 * IKEv2 traffic selector re-negotiation.
	 */
	EVP_DigestInit(&ctx, EVP_sha1());
	TAILQ_FOREACH(prop, &sa->sa_proposals, prop_entry) {
		if (ikev2_valid_proposal(prop, NULL, NULL) != 0)
			continue;

		TAILQ_FOREACH(flow, &sa->sa_policy->pol_flows, flow_entry) {
			/* Use the inbound flow to generate the hash */
			i = IPSP_DIRECTION_IN;
			EVP_DigestUpdate(&ctx, &i, sizeof(i));
			EVP_DigestUpdate(&ctx, &flow->flow_src,
			    sizeof(flow->flow_src));
			EVP_DigestUpdate(&ctx, &flow->flow_dst,
			    sizeof(flow->flow_dst));
		}
	}
	mdlen = sizeof(sa->sa_flowhash);
	EVP_DigestFinal(&ctx, md, &mdlen);

	/* Check the existing flows */
	if (rekey != NULL) {
		if (memcmp(&sa->sa_flowhash, &md, sizeof(md)) == 0) {
			flowloaded = 0;
			TAILQ_FOREACH(flow, &sa->sa_flows,
			    flow_entry) {
				/* Mark the flow as unloaded */
				if (flow->flow_peerspi == rekey->spi) {
					flow->flow_loaded = 0;
					flowloaded++;
				}
			}
			log_debug("%s: keeping %d flows",
			    __func__, flowloaded);
		}

		/* XXX Check ESP/AH/IKE */
		if (ikev2_childsa_delete(env, sa,
		    rekey->spi_protoid, rekey->spi, NULL, 1) == -1) {
			log_debug("%s: failed to disable old SA %s",
			    __func__, print_spi(rekey->spi, rekey->spi_size));
			return (-1);
		}
	}

	memcpy(&sa->sa_flowhash, &md, sizeof(md));

	/* Create the new flows */
	TAILQ_FOREACH(prop, &sa->sa_proposals, prop_entry) {
		if (ikev2_valid_proposal(prop, NULL, NULL) != 0)
			continue;

		TAILQ_FOREACH(flow, &sa->sa_policy->pol_flows, flow_entry) {
			if ((flowa = calloc(1, sizeof(*flowa))) == NULL) {
				log_debug("%s: failed to get flow", __func__);
				goto done;
			}

			memcpy(flowa, flow, sizeof(*flow));
			flowa->flow_dir = IPSP_DIRECTION_IN;
			flowa->flow_saproto = prop->prop_protoid;
			flowa->flow_srcid = localid;
			flowa->flow_dstid = peerid;
			flowa->flow_local = &sa->sa_local;
			flowa->flow_peer = &sa->sa_peer;
			flowa->flow_ikesa = sa;
			flowa->flow_peerspi = prop->prop_peerspi.spi;
			if (flowloaded)
				flowa->flow_loaded = 1;

			TAILQ_INSERT_TAIL(&sa->sa_flows, flowa, flow_entry);

			if ((flowb = calloc(1, sizeof(*flowb))) == NULL) {
				log_debug("%s: failed to get flow", __func__);
				goto done;
			}

			memcpy(flowb, flowa, sizeof(*flow));

			flowb->flow_dir = IPSP_DIRECTION_OUT;
			memcpy(&flowb->flow_src, &flow->flow_dst,
			    sizeof(flow->flow_dst));
			memcpy(&flowb->flow_dst, &flow->flow_src,
			    sizeof(flow->flow_src));

			TAILQ_INSERT_TAIL(&sa->sa_flows, flowb, flow_entry);
		}
	}

	/* Create the CHILD SAs using the key material */
	TAILQ_FOREACH(prop, &sa->sa_proposals, prop_entry) {
		if (ikev2_valid_proposal(prop, &encrxf, &integrxf) != 0)
			continue;

		spi = 0;

		if ((csa = calloc(1, sizeof(*csa))) == NULL) {
			log_debug("%s: failed to get CHILD SA", __func__);
			goto done;
		}

		csa->csa_saproto = prop->prop_protoid;
		csa->csa_ikesa = sa;
		csa->csa_srcid = localid;
		csa->csa_dstid = peerid;
		csa->csa_peerspi = prop->prop_peerspi.spi;
		if (sa->sa_hdr.sh_initiator) {
			/* Initiator -> Responder */
			csa->csa_dir = IPSP_DIRECTION_OUT;
			csa->csa_local = &sa->sa_local;
			csa->csa_peer = &sa->sa_peer;

			if ((ret = pfkey_sa_init(env->sc_pfkey,
			    csa, &spi)) != 0)
				goto done;
			csa->csa_spi.spi = prop->prop_localspi.spi = spi;
		} else {
			/* Responder <- Initiator */
			csa->csa_dir = IPSP_DIRECTION_IN;
			csa->csa_spi.spi = prop->prop_peerspi.spi;
			csa->csa_local = &sa->sa_peer;
			csa->csa_peer = &sa->sa_local;
		}

		if ((csa->csa_encrkey = ibuf_copy(keymat,
		    encrxf->xform_keylength / 8)) == NULL ||
		    (csa->csa_integrkey = ibuf_copy(keymat,
		    integrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA keys", __func__);
			childsa_free(csa);
			goto done;
		}
		csa->csa_encrxf = encrxf;
		csa->csa_integrxf = integrxf;

		TAILQ_INSERT_TAIL(&sa->sa_childsas, csa, csa_entry);

		if ((csb = calloc(1, sizeof(*csb))) == NULL) {
			log_debug("%s: failed to get CHILD SA", __func__);
			goto done;
		}

		memcpy(csb, csa, sizeof(*csb));
		csb->csa_dir = csa->csa_dir == IPSP_DIRECTION_IN ?
		    IPSP_DIRECTION_OUT : IPSP_DIRECTION_IN;
		if (spi == 0) {
			if ((ret = pfkey_sa_init(env->sc_pfkey,
			    csa, &spi)) != 0)
				goto done;
			csa->csa_spi.spi = prop->prop_localspi.spi = spi;
		}
		csb->csa_local = csa->csa_peer;
		csb->csa_peer = csa->csa_local;

		if ((csb->csa_encrkey = ibuf_copy(keymat,
		    encrxf->xform_keylength / 8)) == NULL ||
		    (csb->csa_integrkey = ibuf_copy(keymat,
		    integrxf->xform_keylength / 8)) == NULL) {
			log_debug("%s: failed to get CHILD SA keys", __func__);
			childsa_free(csb);
			goto done;
		}

		TAILQ_INSERT_TAIL(&sa->sa_childsas, csb, csa_entry);
	}

	ret = 0;
 done:
	ibuf_release(keymat);
	ibuf_release(seed);

	return (ret);
}

int
ikev2_childsa_enable(struct iked *env, struct iked_sa *sa)
{
	struct iked_childsa	*csa;
	struct iked_flow	*flow;

	TAILQ_FOREACH(csa, &sa->sa_childsas, csa_entry) {
		if (csa->csa_rekey)
			continue;

		if (pfkey_sa_add(env->sc_pfkey, csa, NULL) != 0) {
			log_debug("%s: failed to load CHILD SA spi %s",
			    __func__, print_spi(csa->csa_spi.spi, 4));
			return (-1);
		}

		log_debug("%s: loaded CHILD SA spi %s", __func__,
		    print_spi(csa->csa_spi.spi, 4));
	}

	TAILQ_FOREACH(flow, &sa->sa_flows, flow_entry) { 
		if (flow->flow_rekey)
			continue;

		if (pfkey_flow_add(env->sc_pfkey, flow) != 0) {
			log_debug("%s: failed to load flow", __func__);
			return (-1);
		}

		log_debug("%s: loaded flow %p", __func__, flow);
	}

	return (0);
}

int
ikev2_childsa_delete(struct iked *env, struct iked_sa *sa, u_int8_t saproto,
    u_int64_t spi, u_int64_t *spiptr, int rekey)
{
	struct iked_childsa	*csa, *nextcsa;
	struct iked_flow	*flow, *nextflow;
	int			 found = 0;
	u_int64_t		 localspi = 0;
	const char		*action;

	if (spiptr)
		*spiptr = 0;

	action = rekey ? "disable" : "delete";

	for (csa = TAILQ_FIRST(&sa->sa_childsas); csa != NULL; csa = nextcsa) {
		nextcsa = TAILQ_NEXT(csa, csa_entry);

		if (csa->csa_saproto != saproto ||
		    csa->csa_peerspi != spi)
			continue;

		if (csa->csa_spi.spi != spi)
			localspi = csa->csa_spi.spi;
		if (pfkey_sa_delete(env->sc_pfkey, csa) != 0)
			log_debug("%s: failed to %s CHILD SA spi %s",
			    __func__, action, print_spi(csa->csa_spi.spi, 4));
		else
			log_debug("%s: %sd CHILD SA spi %s", __func__,
			    action, print_spi(csa->csa_spi.spi, 4));
		found++;

		if (rekey) {
			csa->csa_rekey = 1;
			continue;
		}
		TAILQ_REMOVE(&sa->sa_childsas, csa, csa_entry);
		childsa_free(csa);
	}

	for (flow = TAILQ_FIRST(&sa->sa_flows); flow != NULL; flow = nextflow) {
		nextflow = TAILQ_NEXT(flow, flow_entry);

		if (flow->flow_saproto != saproto ||
		    flow->flow_peerspi != spi)
			continue;
		if (pfkey_flow_delete(env->sc_pfkey, flow) != 0)
			log_debug("%s: failed to %s flow %p",
			    __func__, action, flow);
		else
			log_debug("%s: %sd flow %p",
			    __func__, action, flow);
		found++;

		if (rekey) {
			flow->flow_rekey = 1;
			continue;
		}
		TAILQ_REMOVE(&sa->sa_flows, flow, flow_entry);
		flow_free(flow);
	}

	if (spiptr)
		*spiptr = localspi;

	return (found ? 0 : -1);
}

int
ikev2_valid_proposal(struct iked_proposal *prop,
    struct iked_transform **exf, struct iked_transform **ixf)
{
	struct iked_transform	*xform, *encrxf, *integrxf;
	size_t			 i;

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
		if (xform->xform_type ==
		    IKEV2_XFORMTYPE_ENCR)
			encrxf = xform;
		else if (xform->xform_type ==
		    IKEV2_XFORMTYPE_INTEGR)
			integrxf = xform;
	}
	/* XXX support non-auth / non-enc proposals */
	if (encrxf == NULL || integrxf == NULL)
		return (-1);

	if (exf)
		*exf = encrxf;
	if (ixf)
		*ixf = integrxf;

	return (0);
}

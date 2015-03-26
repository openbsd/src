/*	$OpenBSD: ikev2_msg.c,v 1.42 2015/03/26 19:52:35 markus Exp $	*/

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

void	 ikev2_msg_response_timeout(struct iked *, void *);
void	 ikev2_msg_retransmit_timeout(struct iked *, void *);

void
ikev2_msg_cb(int fd, short event, void *arg)
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
	msg.msg_parent = &msg;
	memcpy(&msg.msg_local, &sock->sock_addr, sizeof(sock->sock_addr));

	if ((len = recvfromto(fd, buf, sizeof(buf), 0,
	    (struct sockaddr *)&msg.msg_peer, &msg.msg_peerlen,
	    (struct sockaddr *)&msg.msg_local, &msg.msg_locallen)) <
	    (ssize_t)sizeof(natt))
		return;

	if (socket_getport((struct sockaddr *)&msg.msg_local) ==
	    IKED_NATT_PORT) {
		if (memcmp(&natt, buf, sizeof(natt)) != 0)
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

		proc_composev_imsg(&env->sc_ps, PROC_IKEV1, -1,
		    IMSG_IKE_MESSAGE, -1, iov, 2);
		goto done;
	}
	TAILQ_INIT(&msg.msg_proposals);

	msg.msg_fd = fd;
	ikev2_recv(env, &msg);

 done:
	ikev2_msg_cleanup(env, &msg);
}

struct ibuf *
ikev2_msg_init(struct iked *env, struct iked_message *msg,
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
	msg->msg_e = 0;
	msg->msg_parent = msg;	/* has to be set */
	TAILQ_INIT(&msg->msg_proposals);

	return (msg->msg_data);
}

struct iked_message *
ikev2_msg_copy(struct iked *env, struct iked_message *msg)
{
	struct iked_message		*m = NULL;
	struct ibuf			*buf;
	size_t				 len;
	void				*ptr;

	if (ibuf_size(msg->msg_data) < msg->msg_offset)
		return (NULL);
	len = ibuf_size(msg->msg_data) - msg->msg_offset;

	if ((ptr = ibuf_seek(msg->msg_data, msg->msg_offset, len)) == NULL ||
	    (m = malloc(sizeof(*m))) == NULL ||
	    (buf = ikev2_msg_init(env, m, &msg->msg_peer, msg->msg_peerlen,
	     &msg->msg_local, msg->msg_locallen, msg->msg_response)) == NULL ||
	    ibuf_add(buf, ptr, len))
		return (NULL);

	m->msg_fd = msg->msg_fd;
	m->msg_msgid = msg->msg_msgid;
	m->msg_offset = msg->msg_offset;
	m->msg_sa = msg->msg_sa;

	return (m);
}

void
ikev2_msg_cleanup(struct iked *env, struct iked_message *msg)
{
	if (msg == msg->msg_parent) {
		ibuf_release(msg->msg_nonce);
		ibuf_release(msg->msg_ke);
		ibuf_release(msg->msg_auth.id_buf);
		ibuf_release(msg->msg_id.id_buf);
		ibuf_release(msg->msg_cert.id_buf);

		config_free_proposals(&msg->msg_proposals, 0);
	}

	if (msg->msg_data != NULL) {
		ibuf_release(msg->msg_data);
		msg->msg_data = NULL;
	}
}

int
ikev2_msg_valid_ike_sa(struct iked *env, struct ike_header *oldhdr,
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

	if (msg->msg_sa != NULL && msg->msg_policy != NULL) {
		/*
		 * Only permit informational requests from initiator
		 * on closing SAs (for DELETE).
		 */
		if (msg->msg_sa->sa_state == IKEV2_STATE_CLOSING) {
			if (((oldhdr->ike_flags &
			    (IKEV2_FLAG_INITIATOR|IKEV2_FLAG_RESPONSE)) ==
			    IKEV2_FLAG_INITIATOR) &&
			    (oldhdr->ike_exchange ==
			    IKEV2_EXCHANGE_INFORMATIONAL))
				return (0);
			return (-1);
		}
		return (0);
	}

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
	if ((buf = ikev2_msg_init(env, &resp,
	    &msg->msg_peer, msg->msg_peerlen,
	    &msg->msg_local, msg->msg_locallen, 1)) == NULL)
		goto done;

	resp.msg_fd = msg->msg_fd;

	bzero(&sa, sizeof(sa));
	if ((oldhdr->ike_flags & IKEV2_FLAG_INITIATOR) == 0)
		sa.sa_hdr.sh_initiator = 1;
	sa.sa_hdr.sh_ispi = betoh64(oldhdr->ike_ispi);
	sa.sa_hdr.sh_rspi = betoh64(oldhdr->ike_rspi);

	resp.msg_msgid = betoh32(oldhdr->ike_msgid);

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, &sa, resp.msg_msgid,
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

	(void)ikev2_pld_parse(env, hdr, &resp, 0);
	(void)ikev2_msg_send(env, &resp);

 done:
	ikev2_msg_cleanup(env, &resp);
#endif

	/* Always fail */
	return (-1);
}

int
ikev2_msg_send(struct iked *env, struct iked_message *msg)
{
	struct iked_sa		*sa = msg->msg_sa;
	struct ibuf		*buf = msg->msg_data;
	u_int32_t		 natt = 0x00000000;
	int			 isnatt = 0;
	u_int8_t		 exchange, flags;
	struct ike_header	*hdr;
	struct iked_message	*m;

	if (buf == NULL || (hdr = ibuf_seek(msg->msg_data,
	    msg->msg_offset, sizeof(*hdr))) == NULL)
		return (-1);

	isnatt = (msg->msg_natt || (msg->msg_sa && msg->msg_sa->sa_natt));

	exchange = hdr->ike_exchange;
	flags = hdr->ike_flags;
	log_info("%s: %s %s from %s to %s msgid %u, %ld bytes%s", __func__,
	    print_map(exchange, ikev2_exchange_map),
	    (flags & IKEV2_FLAG_RESPONSE) ? "response" : "request",
	    print_host((struct sockaddr *)&msg->msg_local, NULL, 0),
	    print_host((struct sockaddr *)&msg->msg_peer, NULL, 0),
	    betoh32(hdr->ike_msgid),
	    ibuf_length(buf), isnatt ? ", NAT-T" : "");

	if (isnatt) {
		if (ibuf_prepend(buf, &natt, sizeof(natt)) == -1) {
			log_debug("%s: failed to set NAT-T", __func__);
			return (-1);
		}
		msg->msg_offset += sizeof(natt);
	}

	if ((sendto(msg->msg_fd, ibuf_data(buf), ibuf_size(buf), 0,
	    (struct sockaddr *)&msg->msg_peer, msg->msg_peerlen)) == -1) {
		log_warn("%s: sendto", __func__);
		return (-1);
	}

	if (!sa)
		return (0);

	if ((m = ikev2_msg_copy(env, msg)) == NULL) {
		log_debug("%s: failed to copy a message", __func__);
		return (-1);
	}
	m->msg_exchange = exchange;

	if (flags & IKEV2_FLAG_RESPONSE) {
		TAILQ_INSERT_TAIL(&sa->sa_responses, m, msg_entry);
		timer_set(env, &m->msg_timer, ikev2_msg_response_timeout, m);
		timer_add(env, &m->msg_timer, IKED_RESPONSE_TIMEOUT);
	} else {
		TAILQ_INSERT_TAIL(&sa->sa_requests, m, msg_entry);
		timer_set(env, &m->msg_timer, ikev2_msg_retransmit_timeout, m);
		timer_add(env, &m->msg_timer, IKED_RETRANSMIT_TIMEOUT);
	}

	return (0);
}

u_int32_t
ikev2_msg_id(struct iked *env, struct iked_sa *sa)
{
	u_int32_t		id = sa->sa_reqid;

	if (++sa->sa_reqid == UINT32_MAX) {
		/* XXX we should close and renegotiate the connection now */
		log_debug("%s: IKEv2 message sequence overflow", __func__);
	}
	return (id);
}

struct ibuf *
ikev2_msg_encrypt(struct iked *env, struct iked_sa *sa, struct ibuf *src)
{
	size_t			 len, ivlen, encrlen, integrlen, blocklen,
				    outlen;
	u_int8_t		*buf, pad = 0, *ptr;
	struct ibuf		*encr, *dst = NULL, *out = NULL;

	buf = ibuf_data(src);
	len = ibuf_size(src);

	log_debug("%s: decrypted length %zu", __func__, len);
	print_hex(buf, 0, len);

	if (sa == NULL ||
	    sa->sa_encr == NULL ||
	    sa->sa_integr == NULL) {
		log_debug("%s: invalid SA", __func__);
		goto done;
	}

	if (sa->sa_hdr.sh_initiator)
		encr = sa->sa_key_iencr;
	else
		encr = sa->sa_key_rencr;

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

	log_debug("%s: padded length %zu", __func__, ibuf_size(src));
	print_hex(ibuf_data(src), 0, ibuf_size(src));

	cipher_setkey(sa->sa_encr, encr->buf, ibuf_length(encr));
	cipher_setiv(sa->sa_encr, NULL, 0);	/* XXX ivlen */
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

	if ((ptr = ibuf_advance(dst, integrlen)) == NULL)
		goto done;
	explicit_bzero(ptr, integrlen);

	log_debug("%s: length %zu, padding %d, output length %zu",
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
ikev2_msg_integr(struct iked *env, struct iked_sa *sa, struct ibuf *src)
{
	int			 ret = -1;
	size_t			 integrlen, tmplen;
	struct ibuf		*integr, *tmp = NULL;
	u_int8_t		*ptr;

	log_debug("%s: message length %zu", __func__, ibuf_size(src));
	print_hex(ibuf_data(src), 0, ibuf_size(src));

	if (sa == NULL ||
	    sa->sa_integr == NULL) {
		log_debug("%s: invalid SA", __func__);
		return (-1);
	}

	if (sa->sa_hdr.sh_initiator)
		integr = sa->sa_key_iauth;
	else
		integr = sa->sa_key_rauth;

	integrlen = hash_length(sa->sa_integr);

	log_debug("%s: integrity checksum length %zu", __func__,
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
ikev2_msg_decrypt(struct iked *env, struct iked_sa *sa,
    struct ibuf *msg, struct ibuf *src)
{
	ssize_t			 ivlen, encrlen, integrlen, blocklen,
				    outlen, tmplen;
	u_int8_t		 pad = 0, *ptr;
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

	if (encrlen < 0 || integroff < 0) {
		log_debug("%s: invalid integrity value", __func__);
		goto done;
	}

	log_debug("%s: IV length %zd", __func__, ivlen);
	print_hex(ibuf_data(src), 0, ivlen);
	log_debug("%s: encrypted payload length %zd", __func__, encrlen);
	print_hex(ibuf_data(src), encroff, encrlen);
	log_debug("%s: integrity checksum length %zd", __func__, integrlen);
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

	log_debug("%s: integrity check succeeded", __func__);
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

	if ((outlen = ibuf_length(out)) != 0) {
		cipher_update(sa->sa_encr, ibuf_data(src) + encroff, encrlen,
		    ibuf_data(out), &outlen);

		ptr = ibuf_seek(out, outlen - 1, 1);
		pad = *ptr;
	}

	log_debug("%s: decrypted payload length %zd/%zd padding %d",
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
ikev2_msg_send_encrypt(struct iked *env, struct iked_sa *sa, struct ibuf **ep,
    u_int8_t exchange, u_int8_t firstpayload, int response)
{
	struct iked_message		 resp;
	struct ike_header		*hdr;
	struct ikev2_payload		*pld;
	struct ibuf			*buf, *e = *ep;
	int				 ret = -1;

	if ((buf = ikev2_msg_init(env, &resp, &sa->sa_peer.addr,
	    sa->sa_peer.addr.ss_len, &sa->sa_local.addr,
	    sa->sa_local.addr.ss_len, response)) == NULL)
		goto done;

	resp.msg_msgid = response ? sa->sa_msgid : ikev2_msg_id(env, sa);

	/* IKE header */
	if ((hdr = ikev2_add_header(buf, sa, resp.msg_msgid, IKEV2_PAYLOAD_SK,
	    exchange, response ? IKEV2_FLAG_RESPONSE : 0)) == NULL)
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
	if (ikev2_next_payload(pld, ibuf_size(e), firstpayload) == -1)
		goto done;

	if (ikev2_set_header(hdr, ibuf_size(buf) - sizeof(*hdr)) == -1)
		goto done;

	/* Add integrity checksum (HMAC) */
	if (ikev2_msg_integr(env, sa, buf) != 0) {
		log_debug("%s: integrity checksum failed", __func__);
		goto done;
	}

	resp.msg_data = buf;
	resp.msg_sa = sa;
	resp.msg_fd = sa->sa_fd;
	TAILQ_INIT(&resp.msg_proposals);

	(void)ikev2_pld_parse(env, hdr, &resp, 0);

	ret = ikev2_msg_send(env, &resp);

 done:
	/* e is cleaned up by the calling function */
	*ep = e;
	ikev2_msg_cleanup(env, &resp);

	return (ret);
}

struct ibuf *
ikev2_msg_auth(struct iked *env, struct iked_sa *sa, int response)
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

	log_debug("%s: %s auth data length %zu",
	    __func__, response ? "responder" : "initiator",
	    ibuf_size(authmsg));
	print_hex(ibuf_data(authmsg), 0, ibuf_size(authmsg));

	return (authmsg);

 fail:
	ibuf_release(authmsg);
	return (NULL);
}

int
ikev2_msg_authverify(struct iked *env, struct iked_sa *sa,
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
		if (!id->id_type || !ibuf_length(id->id_buf)) {
			log_debug("%s: no cert found", __func__);
			goto done;
		}
		key = ibuf_data(id->id_buf);
		keylen = ibuf_size(id->id_buf);
		keytype = id->id_type;
		break;
	}

	log_debug("%s: method %s keylen %zd type %s", __func__,
	    print_map(auth->auth_method, ikev2_auth_map), keylen,
	    print_map(id->id_type, ikev2_cert_map));

	if (dsa_setkey(dsa, key, keylen, keytype) == NULL ||
	    dsa_init(dsa, buf, len) != 0 ||
	    dsa_update(dsa, ibuf_data(authmsg), ibuf_size(authmsg))) {
		log_debug("%s: failed to compute digital signature", __func__);
		goto done;
	}

	if ((ret = dsa_verify_final(dsa, buf, len)) == 0) {
		log_debug("%s: authentication successful", __func__);
		sa_state(env, sa, IKEV2_STATE_AUTH_SUCCESS);
		sa_stateflags(sa, IKED_REQ_AUTHVALID);
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
ikev2_msg_authsign(struct iked *env, struct iked_sa *sa,
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
	    dsa_init(dsa, NULL, 0) != 0 ||
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
ikev2_msg_frompeer(struct iked_message *msg)
{
	struct iked_sa		*sa = msg->msg_sa;
	struct ike_header	*hdr;

	msg = msg->msg_parent;

	if (sa == NULL ||
	    (hdr = ibuf_seek(msg->msg_data, 0, sizeof(*hdr))) == NULL)
		return (0);

	if (!sa->sa_hdr.sh_initiator &&
	    (hdr->ike_flags & IKEV2_FLAG_INITIATOR))
		return (1);
	else if (sa->sa_hdr.sh_initiator &&
	    (hdr->ike_flags & IKEV2_FLAG_INITIATOR) == 0)
		return (1);

	return (0);
}

struct iked_socket *
ikev2_msg_getsocket(struct iked *env, int af, int natt)
{
	switch (af) {
	case AF_INET:
		return (env->sc_sock4[natt ? 1 : 0]);
	case AF_INET6:
		return (env->sc_sock6[natt ? 1 : 0]);
	}

	log_debug("%s: af socket %d not available", __func__, af);
	return (NULL);
}

void
ikev2_msg_prevail(struct iked *env, struct iked_msgqueue *queue,
    struct iked_message *msg)
{
	struct iked_message	*m, *mtmp;

	TAILQ_FOREACH_SAFE(m, queue, msg_entry, mtmp) {
		if (m->msg_msgid < msg->msg_msgid)
			ikev2_msg_dispose(env, queue, m);
	}
}

void
ikev2_msg_dispose(struct iked *env, struct iked_msgqueue *queue,
    struct iked_message *msg)
{
	TAILQ_REMOVE(queue, msg, msg_entry);
	timer_del(env, &msg->msg_timer);
	ikev2_msg_cleanup(env, msg);
	free(msg);
}

void
ikev2_msg_flushqueue(struct iked *env, struct iked_msgqueue *queue)
{
	struct iked_message	*m = NULL;

	while ((m = TAILQ_FIRST(queue)) != NULL)
		ikev2_msg_dispose(env, queue, m);
}

struct iked_message *
ikev2_msg_lookup(struct iked *env, struct iked_msgqueue *queue,
    struct iked_message *msg, struct ike_header *hdr)
{
	struct iked_message	*m = NULL;

	TAILQ_FOREACH(m, queue, msg_entry) {
		if (m->msg_msgid == msg->msg_msgid &&
		    m->msg_exchange == hdr->ike_exchange)
			break;
	}

	return (m);
}

int
ikev2_msg_retransmit_response(struct iked *env, struct iked_sa *sa,
    struct iked_message *msg)
{
	if ((sendto(msg->msg_fd, ibuf_data(msg->msg_data),
	    ibuf_size(msg->msg_data), 0, (struct sockaddr *)&msg->msg_peer,
	    msg->msg_peerlen)) == -1) {
		log_warn("%s: sendto", __func__);
		return (-1);
	}

	timer_add(env, &msg->msg_timer, IKED_RESPONSE_TIMEOUT);
	return (0);
}

void
ikev2_msg_response_timeout(struct iked *env, void *arg)
{
	struct iked_message	*msg = arg;
	struct iked_sa		*sa = msg->msg_sa;

	ikev2_msg_dispose(env, &sa->sa_responses, msg);
}

void
ikev2_msg_retransmit_timeout(struct iked *env, void *arg)
{
	struct iked_message	*msg = arg;
	struct iked_sa		*sa = msg->msg_sa;

	if (msg->msg_tries < IKED_RETRANSMIT_TRIES) {
		if ((sendto(msg->msg_fd, ibuf_data(msg->msg_data),
		    ibuf_size(msg->msg_data), 0,
		    (struct sockaddr *)&msg->msg_peer,
		    msg->msg_peerlen)) == -1) {
			log_warn("%s: sendto", __func__);
			sa_free(env, sa);
			return;
		}
		/* Exponential timeout */
		timer_add(env, &msg->msg_timer,
		    IKED_RETRANSMIT_TIMEOUT * (2 << (msg->msg_tries++)));
	} else {
		log_debug("%s: retransmit limit reached for msgid %u",
		    __func__, msg->msg_msgid);
		sa_free(env, sa);
	}
}

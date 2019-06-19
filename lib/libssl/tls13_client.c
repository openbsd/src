/* $OpenBSD: tls13_client.c,v 1.16 2019/04/05 20:23:38 tb Exp $ */
/*
 * Copyright (c) 2018, 2019 Joel Sing <jsing@openbsd.org>
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

#include "ssl_locl.h"

#include <openssl/curve25519.h>
#include <openssl/ssl3.h>

#include "bytestring.h"
#include "ssl_tlsext.h"
#include "tls13_handshake.h"
#include "tls13_internal.h"

int
tls13_connect(struct tls13_ctx *ctx)
{
	if (ctx->mode != TLS13_HS_CLIENT)
		return TLS13_IO_FAILURE;

	return tls13_handshake_perform(ctx);
}

static int
tls13_client_init(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;

	if (!ssl_supported_version_range(s, &ctx->hs->min_version,
	    &ctx->hs->max_version)) {
		SSLerror(s, SSL_R_NO_PROTOCOLS_AVAILABLE);
		return 0;
	}
	s->client_version = s->version = ctx->hs->max_version;

	if (!ssl_get_new_session(s, 0)) /* XXX */
		return 0;

	if (!tls1_transcript_init(s))
		return 0;

	arc4random_buf(s->s3->client_random, SSL3_RANDOM_SIZE);

	return 1;
}

int
tls13_legacy_connect(SSL *ssl)
{
	struct tls13_ctx *ctx = ssl->internal->tls13;
	int ret;

	if (ctx == NULL) {
		if ((ctx = tls13_ctx_new(TLS13_HS_CLIENT)) == NULL) {
			SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
		ssl->internal->tls13 = ctx;
		ctx->ssl = ssl;
		ctx->hs = &S3I(ssl)->hs_tls13;

		if (!tls13_client_init(ctx)) {
			if (ERR_peek_error() == 0)
				SSLerror(ssl, ERR_R_INTERNAL_ERROR); /* XXX */
			return -1;
		}
	}

	S3I(ssl)->hs.state = SSL_ST_CONNECT;

	ret = tls13_connect(ctx);
	if (ret == TLS13_IO_USE_LEGACY)
		return ssl->method->internal->ssl_connect(ssl);
	if (ret == TLS13_IO_SUCCESS)
		S3I(ssl)->hs.state = SSL_ST_OK;

	return tls13_legacy_return_code(ssl, ret);
}

int
tls13_use_legacy_client(struct tls13_ctx *ctx)
{
	SSL *s = ctx->ssl;
	CBS cbs;

	s->method = tls_legacy_client_method();
	s->client_version = s->version = s->method->internal->max_version;

	if (!ssl3_setup_init_buffer(s))
		goto err;
	if (!ssl3_setup_buffers(s))
		goto err;
	if (!ssl_init_wbio_buffer(s, 0))
		goto err;

	if (s->bbio != s->wbio)
		s->wbio = BIO_push(s->bbio, s->wbio);

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!BUF_MEM_grow_clean(s->internal->init_buf, CBS_len(&cbs) + 4))
		goto err;

	if (!CBS_write_bytes(&cbs, s->internal->init_buf->data + 4,
	    s->internal->init_buf->length - 4, NULL))
		goto err;

	S3I(s)->tmp.reuse_message = 1;
	S3I(s)->tmp.message_type = tls13_handshake_msg_type(ctx->hs_msg);
	S3I(s)->tmp.message_size = CBS_len(&cbs);

	S3I(s)->hs.state = SSL3_ST_CR_SRVR_HELLO_A;

	return 1;

 err:
	return 0;
}

static int
tls13_client_hello_build(SSL *s, CBB *cbb)
{
	CBB cipher_suites, compression_methods, session_id;
	uint8_t *sid;

	if (!CBB_add_u16(cbb, TLS1_2_VERSION))
		goto err;
	if (!CBB_add_bytes(cbb, s->s3->client_random, SSL3_RANDOM_SIZE))
		goto err;

	/* Either 32-random bytes or zero length... */
	/* XXX - session resumption for TLSv1.2? */
	if (!CBB_add_u8_length_prefixed(cbb, &session_id))
		goto err;
	if (!CBB_add_space(&session_id, &sid, 32))
		goto err;
	arc4random_buf(sid, 32);

	if (!CBB_add_u16_length_prefixed(cbb, &cipher_suites))
		goto err;
	if (!ssl_cipher_list_to_bytes(s, SSL_get_ciphers(s), &cipher_suites)) {
		SSLerror(s, SSL_R_NO_CIPHERS_AVAILABLE);
		goto err;
	}

	if (!CBB_add_u8_length_prefixed(cbb, &compression_methods))
		goto err;
	if (!CBB_add_u8(&compression_methods, 0))
		goto err;

	if (!tlsext_client_build(s, cbb, SSL_TLSEXT_MSG_CH))
		goto err;

	if (!CBB_flush(cbb))
		goto err;

	return 1;

 err:
	return 0;
}

int
tls13_client_hello_send(struct tls13_ctx *ctx)
{
	CBB body;

	if (!tls13_handshake_msg_start(ctx->hs_msg, &body, TLS13_MT_CLIENT_HELLO))
		return 0;
	if (!tls13_client_hello_build(ctx->ssl, &body))
		return 0;
	if (!tls13_handshake_msg_finish(ctx->hs_msg))
		return 0;

	return 1;
}

/*
 * HelloRetryRequest hash - RFC 8446 section 4.1.3.
 */
static const uint8_t tls13_hello_retry_request_hash[] = {
	0xcf, 0x21, 0xad, 0x74, 0xe5, 0x9a, 0x61, 0x11,
	0xbe, 0x1d, 0x8c, 0x02, 0x1e, 0x65, 0xb8, 0x91,
	0xc2, 0xa2, 0x11, 0x16, 0x7a, 0xbb, 0x8c, 0x5e,
	0x07, 0x9e, 0x09, 0xe2, 0xc8, 0xa8, 0x33, 0x9c,
};

static int
tls13_server_hello_is_legacy(CBS *cbs)
{
	CBS extensions_block, extensions, extension_data;
	uint16_t selected_version = 0;
	uint16_t type;

	CBS_dup(cbs, &extensions_block);

	if (!CBS_get_u16_length_prefixed(&extensions_block, &extensions))
		return 1;

	while (CBS_len(&extensions) > 0) {
		if (!CBS_get_u16(&extensions, &type))
			return 1;
		if (!CBS_get_u16_length_prefixed(&extensions, &extension_data))
			return 1;

		if (type != TLSEXT_TYPE_supported_versions)
			continue;
		if (!CBS_get_u16(&extension_data, &selected_version))
			return 1;
		if (CBS_len(&extension_data) != 0)
			return 1;
	}

	return (selected_version < TLS1_3_VERSION);
}

static int
tls13_server_hello_process(struct tls13_ctx *ctx, CBS *cbs)
{
	CBS server_random, session_id;
	uint16_t cipher_suite, legacy_version;
	uint8_t compression_method;
	const SSL_CIPHER *cipher;
	SSL *s = ctx->ssl;
	int alert;

	if (!CBS_get_u16(cbs, &legacy_version))
		goto err;
	if (!CBS_get_bytes(cbs, &server_random, SSL3_RANDOM_SIZE))
		goto err;
	if (!CBS_get_u8_length_prefixed(cbs, &session_id))
		goto err;
	if (!CBS_get_u16(cbs, &cipher_suite))
		goto err;
	if (!CBS_get_u8(cbs, &compression_method))
		goto err;

	if (tls13_server_hello_is_legacy(cbs))
		return tls13_use_legacy_client(ctx);

	if (!tlsext_client_parse(s, cbs, &alert, SSL_TLSEXT_MSG_SH))
		goto err;

	if (CBS_len(cbs) != 0)
		goto err;

	/*
	 * See if a supported versions extension was returned. If it was then
	 * the legacy version must be set to 0x0303 (RFC 8446 section 4.1.3).
	 * Otherwise, fallback to the legacy version, ensuring that it is both
	 * within range and not TLS 1.3 or greater (which must use the
	 * supported version extension.
	 */
	if (ctx->hs->server_version != 0) {
		if (legacy_version != TLS1_2_VERSION) {
			/* XXX - alert. */
			goto err;
		}
	} else {
		if (legacy_version < ctx->hs->min_version ||
		    legacy_version > ctx->hs->max_version ||
		    legacy_version > TLS1_2_VERSION) {
			/* XXX - alert. */
			goto err;
		}
		ctx->hs->server_version = legacy_version;
	}

	/* XXX - session_id must match. */

	/*
	 * Ensure that the cipher suite is one that we offered in the client
	 * hello and that it matches the TLS version selected.
	 */
	cipher = ssl3_get_cipher_by_value(cipher_suite);
	if (cipher == NULL ||
	    sk_SSL_CIPHER_find(ssl_get_ciphers_by_id(s), cipher) < 0) {
		/* XXX - alert. */
		goto err;
	}
	if (ctx->hs->server_version == TLS1_3_VERSION &&
	    cipher->algorithm_ssl != SSL_TLSV1_3) {
		/* XXX - alert. */
		goto err;
	}
	/* XXX - move this to hs_tls13? */
	S3I(s)->hs.new_cipher = cipher;

	if (compression_method != 0) {
		/* XXX - alert. */
		goto err;
	}

	if (CBS_mem_equal(&server_random, tls13_hello_retry_request_hash,
	    sizeof(tls13_hello_retry_request_hash)))
		ctx->handshake_stage.hs_type |= WITH_HRR;

	return 1;

 err:
	/* XXX - send alert. */

	return 0;
}

int
tls13_server_hello_recv(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets;
	struct tls13_secret context;
	unsigned char buf[EVP_MAX_MD_SIZE];
	uint8_t *shared_key = NULL;
	size_t hash_len;
	SSL *s = ctx->ssl;
	int ret = 0;
	CBS cbs;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!tls13_server_hello_process(ctx, &cbs))
		goto err;

	/* See if we switched back to the legacy client method. */
	if (s->method->internal->version < TLS1_3_VERSION)
		return 1;

	/* XXX - handle other key share types. */
	if (ctx->hs->x25519_peer_public == NULL) {
		/* XXX - alert. */
		goto err;
	}
	if ((shared_key = malloc(X25519_KEY_LENGTH)) == NULL)
		goto err;
	if (!X25519(shared_key, ctx->hs->x25519_private,
	    ctx->hs->x25519_peer_public))
		goto err;

	s->session->cipher = S3I(s)->hs.new_cipher;
	s->session->ssl_version = ctx->hs->server_version;

	if ((ctx->aead = tls13_cipher_aead(S3I(s)->hs.new_cipher)) == NULL)
		goto err;
	if ((ctx->hash = tls13_cipher_hash(S3I(s)->hs.new_cipher)) == NULL)
		goto err;

	if ((secrets = tls13_secrets_create(ctx->hash, 0)) == NULL)
		goto err;
	S3I(ctx->ssl)->hs_tls13.secrets = secrets;

	/* XXX - pass in hash. */
	if (!tls1_transcript_hash_init(s))
		goto err;
	if (!tls1_transcript_hash_value(s, buf, sizeof(buf), &hash_len))
		goto err;
	context.data = buf;
	context.len = hash_len;

	/* Early secrets. */
	if (!tls13_derive_early_secrets(secrets, secrets->zeros.data,
	    secrets->zeros.len, &context))
		goto err;

	/* Handshake secrets. */
	if (!tls13_derive_handshake_secrets(ctx->hs->secrets, shared_key,
	    X25519_KEY_LENGTH, &context))
		goto err;

	tls13_record_layer_set_aead(ctx->rl, ctx->aead);
	tls13_record_layer_set_hash(ctx->rl, ctx->hash);

	if (!tls13_record_layer_set_read_traffic_key(ctx->rl,
	    &secrets->server_handshake_traffic))
		goto err;
	if (!tls13_record_layer_set_write_traffic_key(ctx->rl,
	    &secrets->client_handshake_traffic))
		goto err;

	ctx->handshake_stage.hs_type |= NEGOTIATED;
	ret = 1;

 err:
	freezero(shared_key, X25519_KEY_LENGTH);
	return ret;
}

int
tls13_server_encrypted_extensions_recv(struct tls13_ctx *ctx)
{
	int alert;
	CBS cbs;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!tlsext_client_parse(ctx->ssl, &cbs, &alert, SSL_TLSEXT_MSG_EE))
		goto err;

	if (CBS_len(&cbs) != 0)
		goto err;

	return 1;

 err:
	/* XXX - send alert. */

	return 0;
}

int
tls13_server_certificate_request_recv(struct tls13_ctx *ctx)
{
	/*
	 * Thanks to poor state design in the RFC, this function can be called
	 * when we actually have a certificate message instead of a certificate
	 * request... in that case we call the certificate handler after
	 * switching state, to avoid advancing state.
	 */
	if (tls13_handshake_msg_type(ctx->hs_msg) == TLS13_MT_CERTIFICATE) {
		ctx->handshake_stage.hs_type |= WITHOUT_CR;
		return tls13_server_certificate_recv(ctx);
	}

	/* XXX - unimplemented. */

	return 0;
}

int
tls13_server_certificate_recv(struct tls13_ctx *ctx)
{
	CBS cbs, cert_request_context, cert_list, cert_data, cert_exts;
	struct stack_st_X509 *certs = NULL;
	SSL *s = ctx->ssl;
	X509 *cert = NULL;
	EVP_PKEY *pkey;
	const uint8_t *p;
	int cert_idx;
	int ret = 0;

	if ((certs = sk_X509_new_null()) == NULL)
		goto err;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!CBS_get_u8_length_prefixed(&cbs, &cert_request_context))
		goto err;
	if (CBS_len(&cert_request_context) != 0)
		goto err;
	if (!CBS_get_u24_length_prefixed(&cbs, &cert_list))
		goto err;
	if (CBS_len(&cbs) != 0)
		goto err;

	while (CBS_len(&cert_list) > 0) {
		if (!CBS_get_u24_length_prefixed(&cert_list, &cert_data))
			goto err;
		if (!CBS_get_u16_length_prefixed(&cert_list, &cert_exts))
			goto err;

		p = CBS_data(&cert_data);
		if ((cert = d2i_X509(NULL, &p, CBS_len(&cert_data))) == NULL)
			goto err;
		if (p != CBS_data(&cert_data) + CBS_len(&cert_data))
			goto err;

		if (!sk_X509_push(certs, cert))
			goto err;

		cert = NULL;
	}

	/*
	 * At this stage we still have no proof of possession. As such, it would
	 * be preferable to keep the chain and verify once we have successfully
	 * processed the CertificateVerify message.
	 */
	if (ssl_verify_cert_chain(s, certs) <= 0 &&
	    s->verify_mode != SSL_VERIFY_NONE) {
		/* XXX send alert */
		goto err;
	}
	ERR_clear_error();

	cert = sk_X509_value(certs, 0);
	X509_up_ref(cert);

	if ((pkey = X509_get0_pubkey(cert)) == NULL)
		goto err;
	if (EVP_PKEY_missing_parameters(pkey))
		goto err;
	if ((cert_idx = ssl_cert_type(cert, pkey)) < 0)
		goto err;

	ssl_sess_cert_free(SSI(s)->sess_cert);
	if ((SSI(s)->sess_cert = ssl_sess_cert_new()) == NULL)
		goto err;

	SSI(s)->sess_cert->cert_chain = certs;
	certs = NULL;

	X509_up_ref(cert);
	SSI(s)->sess_cert->peer_pkeys[cert_idx].x509 = cert;
	SSI(s)->sess_cert->peer_key = &(SSI(s)->sess_cert->peer_pkeys[cert_idx]);

	X509_free(s->session->peer);

	X509_up_ref(cert);
	s->session->peer = cert;
	s->session->verify_result = s->verify_result;

	ret = 1;

 err:
	sk_X509_pop_free(certs, X509_free);
	X509_free(cert);

	return ret;
}

/*
 * Certificate Verify padding - RFC 8446 section 4.4.3.
 */
static uint8_t cert_verify_pad[64] = {
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

static uint8_t server_cert_verify_context[] = "TLS 1.3, server CertificateVerify";

int
tls13_server_certificate_verify_recv(struct tls13_ctx *ctx)
{
	const struct ssl_sigalg *sigalg;
	uint16_t signature_scheme;
	uint8_t *sig_content = NULL;
	size_t sig_content_len;
	EVP_MD_CTX *mdctx = NULL;
	EVP_PKEY_CTX *pctx;
	EVP_PKEY *pkey;
	X509 *cert;
	CBS cbs, signature;
	CBB cbb;
	int ret = 0;

	memset(&cbb, 0, sizeof(cbb));

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	if (!CBS_get_u16(&cbs, &signature_scheme))
		goto err;
	if (!CBS_get_u16_length_prefixed(&cbs, &signature))
		goto err;
	if (CBS_len(&cbs) != 0)
		goto err;

	if ((sigalg = ssl_sigalg(signature_scheme, tls13_sigalgs,
	    tls13_sigalgs_len)) == NULL)
		goto err;

	if (!CBB_init(&cbb, 0))
		goto err;
	if (!CBB_add_bytes(&cbb, cert_verify_pad, sizeof(cert_verify_pad)))
		goto err;
	if (!CBB_add_bytes(&cbb, server_cert_verify_context,
	    strlen(server_cert_verify_context)))
		goto err;
	if (!CBB_add_u8(&cbb, 0))
		goto err;
	if (!CBB_add_bytes(&cbb, ctx->hs->transcript_hash,
	    ctx->hs->transcript_hash_len))
		goto err;
	if (!CBB_finish(&cbb, &sig_content, &sig_content_len))
		goto err;

	if ((cert = ctx->ssl->session->peer) == NULL)
		goto err;
	if ((pkey = X509_get0_pubkey(cert)) == NULL)
		goto err;
	if (!ssl_sigalg_pkey_ok(sigalg, pkey, 1))
		goto err;

	if (CBS_len(&signature) > EVP_PKEY_size(pkey))
		goto err;

	if ((mdctx = EVP_MD_CTX_new()) == NULL)
		goto err;
	if (!EVP_DigestVerifyInit(mdctx, &pctx, sigalg->md(), NULL, pkey))
		goto err;
	if (sigalg->flags & SIGALG_FLAG_RSA_PSS) {
		if (!EVP_PKEY_CTX_set_rsa_padding(pctx, RSA_PKCS1_PSS_PADDING))
			goto err;
		if (!EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1))
			goto err;
	}
	if (!EVP_DigestVerifyUpdate(mdctx, sig_content, sig_content_len))
		goto err;
	if (EVP_DigestVerifyFinal(mdctx, CBS_data(&signature),
	    CBS_len(&signature)) <= 0) {
		/* XXX - send alert. */
		goto err;
	}

	ret = 1;

 err:
	CBB_cleanup(&cbb);
	EVP_MD_CTX_free(mdctx);
	free(sig_content);

	return ret;
}

int
tls13_server_finished_recv(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets = ctx->hs->secrets;
	struct tls13_secret context = { .data = "", .len = 0 };
	struct tls13_secret finished_key;
	uint8_t transcript_hash[EVP_MAX_MD_SIZE];
	size_t transcript_hash_len;
	uint8_t *verify_data = NULL;
	size_t verify_data_len;
	uint8_t key[EVP_MAX_MD_SIZE];
	HMAC_CTX *hmac_ctx = NULL;
	unsigned int hlen;
	int ret = 0;
	CBS cbs;

	if (!tls13_handshake_msg_content(ctx->hs_msg, &cbs))
		goto err;

	/*
	 * Verify server finished.
	 */
	finished_key.data = key;
	finished_key.len = EVP_MD_size(ctx->hash);

	if (!tls13_hkdf_expand_label(&finished_key, ctx->hash,
	    &secrets->server_handshake_traffic, "finished",
	    &context))
		goto err;

	if ((hmac_ctx = HMAC_CTX_new()) == NULL)
		goto err;
	if (!HMAC_Init_ex(hmac_ctx, finished_key.data, finished_key.len,
	    ctx->hash, NULL))
		goto err;
	if (!HMAC_Update(hmac_ctx, ctx->hs->transcript_hash,
	    ctx->hs->transcript_hash_len))
		goto err;
	verify_data_len = HMAC_size(hmac_ctx);
	if ((verify_data = calloc(1, verify_data_len)) == NULL)
		goto err;
	if (!HMAC_Final(hmac_ctx, verify_data, &hlen))
		goto err;
	if (hlen != verify_data_len)
		goto err;

	if (!CBS_mem_equal(&cbs, verify_data, verify_data_len)) {
		/* XXX - send alert. */
		goto err;
	}

	/*
	 * Derive application traffic keys.
	 */
	if (!tls1_transcript_hash_value(ctx->ssl, transcript_hash,
	    sizeof(transcript_hash), &transcript_hash_len))
		goto err;

	context.data = transcript_hash;
	context.len = transcript_hash_len;

	if (!tls13_derive_application_secrets(secrets, &context))
		goto err;

	/*
	 * Any records following the server finished message must be encrypted
	 * using the server application traffic keys.
	 */
	if (!tls13_record_layer_set_read_traffic_key(ctx->rl,
	    &secrets->server_application_traffic))
		goto err;

	ret = 1;

 err:
	HMAC_CTX_free(hmac_ctx);
	free(verify_data);

	return ret;
}

int
tls13_client_finished_send(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets = ctx->hs->secrets;
	struct tls13_secret context = { .data = "", .len = 0 };
	struct tls13_secret finished_key;
	uint8_t transcript_hash[EVP_MAX_MD_SIZE];
	size_t transcript_hash_len;
	uint8_t key[EVP_MAX_MD_SIZE];
	uint8_t *verify_data;
	size_t hmac_len;
	unsigned int hlen;
	HMAC_CTX *hmac_ctx = NULL;
	int ret = 0;
	CBB body;

	finished_key.data = key;
	finished_key.len = EVP_MD_size(ctx->hash);

	if (!tls13_hkdf_expand_label(&finished_key, ctx->hash,
	    &secrets->client_handshake_traffic, "finished",
	    &context))
		goto err;

	if (!tls1_transcript_hash_value(ctx->ssl, transcript_hash,
	    sizeof(transcript_hash), &transcript_hash_len))
		goto err;

	if ((hmac_ctx = HMAC_CTX_new()) == NULL)
		goto err;
	if (!HMAC_Init_ex(hmac_ctx, finished_key.data, finished_key.len,
	    ctx->hash, NULL))
		goto err;
	if (!HMAC_Update(hmac_ctx, transcript_hash, transcript_hash_len))
		goto err;

	if (!tls13_handshake_msg_start(ctx->hs_msg, &body, TLS13_MT_FINISHED))
		goto err;
	hmac_len = HMAC_size(hmac_ctx);
	if (!CBB_add_space(&body, &verify_data, hmac_len))
		goto err;
	if (!HMAC_Final(hmac_ctx, verify_data, &hlen))
		goto err;
	if (hlen != hmac_len)
		goto err;
	if (!tls13_handshake_msg_finish(ctx->hs_msg))
		goto err;

	ret = 1;

 err:
	HMAC_CTX_free(hmac_ctx);

	return ret;
}

int
tls13_client_finished_sent(struct tls13_ctx *ctx)
{
	struct tls13_secrets *secrets = ctx->hs->secrets;

	/*
	 * Any records following the client finished message must be encrypted
	 * using the client application traffic keys.
	 */
	return tls13_record_layer_set_write_traffic_key(ctx->rl,
	    &secrets->client_application_traffic);
}

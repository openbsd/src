/* $OpenBSD: ssl_srvr.c,v 1.101 2021/03/29 16:56:20 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright (c) 1998-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
/* ====================================================================
 * Copyright 2002 Sun Microsystems, Inc. ALL RIGHTS RESERVED.
 *
 * Portions of the attached software ("Contribution") are developed by
 * SUN MICROSYSTEMS, INC., and are contributed to the OpenSSL project.
 *
 * The Contribution is licensed pursuant to the OpenSSL open source
 * license provided above.
 *
 * ECC cipher suite support in OpenSSL originally written by
 * Vipul Gupta and Sumit Gupta of Sun Microsystems Laboratories.
 *
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <stdio.h>

#include "ssl_locl.h"

#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/curve25519.h>
#include <openssl/evp.h>
#include <openssl/dh.h>
#ifndef OPENSSL_NO_GOST
#include <openssl/gost.h>
#endif
#include <openssl/hmac.h>
#include <openssl/md5.h>
#include <openssl/objects.h>
#include <openssl/x509.h>

#include "bytestring.h"
#include "ssl_sigalgs.h"
#include "ssl_tlsext.h"

int
ssl3_accept(SSL *s)
{
	void (*cb)(const SSL *ssl, int type, int val) = NULL;
	unsigned long alg_k;
	int ret = -1;
	int new_state, state, skip = 0;
	int listen = 0;

	ERR_clear_error();
	errno = 0;

	if (s->internal->info_callback != NULL)
		cb = s->internal->info_callback;
	else if (s->ctx->internal->info_callback != NULL)
		cb = s->ctx->internal->info_callback;

	if (SSL_is_dtls(s))
		listen = D1I(s)->listen;

	/* init things to blank */
	s->internal->in_handshake++;
	if (!SSL_in_init(s) || SSL_in_before(s))
		SSL_clear(s);

	if (SSL_is_dtls(s))
		D1I(s)->listen = listen;

	for (;;) {
		state = S3I(s)->hs.state;

		switch (S3I(s)->hs.state) {
		case SSL_ST_RENEGOTIATE:
			s->internal->renegotiate = 1;
			/* S3I(s)->hs.state=SSL_ST_ACCEPT; */

		case SSL_ST_BEFORE:
		case SSL_ST_ACCEPT:
		case SSL_ST_BEFORE|SSL_ST_ACCEPT:
		case SSL_ST_OK|SSL_ST_ACCEPT:
			s->server = 1;
			if (cb != NULL)
				cb(s, SSL_CB_HANDSHAKE_START, 1);

			if (!ssl_legacy_stack_version(s, s->version)) {
				SSLerror(s, ERR_R_INTERNAL_ERROR);
				ret = -1;
				goto end;
			}

			if (!ssl_supported_tls_version_range(s,
			    &S3I(s)->hs.our_min_tls_version,
			    &S3I(s)->hs.our_max_tls_version)) {
				SSLerror(s, SSL_R_NO_PROTOCOLS_AVAILABLE);
				ret = -1;
				goto end;
			}

			if (!ssl3_setup_init_buffer(s)) {
				ret = -1;
				goto end;
			}
			if (!ssl3_setup_buffers(s)) {
				ret = -1;
				goto end;
			}

			s->internal->init_num = 0;

			if (S3I(s)->hs.state != SSL_ST_RENEGOTIATE) {
				/*
				 * Ok, we now need to push on a buffering BIO
				 * so that the output is sent in a way that
				 * TCP likes :-)
				 */
				if (!ssl_init_wbio_buffer(s, 1)) {
					ret = -1;
					goto end;
				}

				if (!tls1_transcript_init(s)) {
					ret = -1;
					goto end;
				}

				S3I(s)->hs.state = SSL3_ST_SR_CLNT_HELLO_A;
				s->ctx->internal->stats.sess_accept++;
			} else if (!SSL_is_dtls(s) && !S3I(s)->send_connection_binding) {
				/*
				 * Server attempting to renegotiate with
				 * client that doesn't support secure
				 * renegotiation.
				 */
				SSLerror(s, SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
				ssl3_send_alert(s, SSL3_AL_FATAL,
				    SSL_AD_HANDSHAKE_FAILURE);
				ret = -1;
				goto end;
			} else {
				/*
				 * S3I(s)->hs.state == SSL_ST_RENEGOTIATE,
				 * we will just send a HelloRequest.
				 */
				s->ctx->internal->stats.sess_accept_renegotiate++;
				S3I(s)->hs.state = SSL3_ST_SW_HELLO_REQ_A;
			}
			break;

		case SSL3_ST_SW_HELLO_REQ_A:
		case SSL3_ST_SW_HELLO_REQ_B:
			s->internal->shutdown = 0;
			if (SSL_is_dtls(s)) {
				dtls1_clear_record_buffer(s);
				dtls1_start_timer(s);
			}
			ret = ssl3_send_hello_request(s);
			if (ret <= 0)
				goto end;
			if (SSL_is_dtls(s))
				S3I(s)->hs.tls12.next_state = SSL3_ST_SR_CLNT_HELLO_A;
			else
				S3I(s)->hs.tls12.next_state = SSL3_ST_SW_HELLO_REQ_C;
			S3I(s)->hs.state = SSL3_ST_SW_FLUSH;
			s->internal->init_num = 0;

			if (SSL_is_dtls(s)) {
				if (!tls1_transcript_init(s)) {
					ret = -1;
					goto end;
				}
			}
			break;

		case SSL3_ST_SW_HELLO_REQ_C:
			S3I(s)->hs.state = SSL_ST_OK;
			break;

		case SSL3_ST_SR_CLNT_HELLO_A:
		case SSL3_ST_SR_CLNT_HELLO_B:
		case SSL3_ST_SR_CLNT_HELLO_C:
			s->internal->shutdown = 0;
			if (SSL_is_dtls(s)) {
				ret = ssl3_get_client_hello(s);
				if (ret <= 0)
					goto end;
				dtls1_stop_timer(s);

				if (ret == 1 &&
				    (SSL_get_options(s) & SSL_OP_COOKIE_EXCHANGE))
					S3I(s)->hs.state = DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A;
				else
					S3I(s)->hs.state = SSL3_ST_SW_SRVR_HELLO_A;

				s->internal->init_num = 0;

				/*
				 * Reflect ClientHello sequence to remain
				 * stateless while listening.
				 */
				if (listen) {
					tls12_record_layer_reflect_seq_num(
					    s->internal->rl);
				}

				/* If we're just listening, stop here */
				if (listen && S3I(s)->hs.state == SSL3_ST_SW_SRVR_HELLO_A) {
					ret = 2;
					D1I(s)->listen = 0;
					/*
					 * Set expected sequence numbers to
					 * continue the handshake.
					 */
					D1I(s)->handshake_read_seq = 2;
					D1I(s)->handshake_write_seq = 1;
					D1I(s)->next_handshake_write_seq = 1;
					goto end;
				}
			} else {
				if (s->internal->rwstate != SSL_X509_LOOKUP) {
					ret = ssl3_get_client_hello(s);
					if (ret <= 0)
						goto end;
				}

				s->internal->renegotiate = 2;
				S3I(s)->hs.state = SSL3_ST_SW_SRVR_HELLO_A;
				s->internal->init_num = 0;
			}
			break;

		case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A:
		case DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B:
			ret = ssl3_send_dtls_hello_verify_request(s);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SW_FLUSH;
			S3I(s)->hs.tls12.next_state = SSL3_ST_SR_CLNT_HELLO_A;

			/* HelloVerifyRequest resets Finished MAC. */
			tls1_transcript_reset(s);
			break;

		case SSL3_ST_SW_SRVR_HELLO_A:
		case SSL3_ST_SW_SRVR_HELLO_B:
			if (SSL_is_dtls(s)) {
				s->internal->renegotiate = 2;
				dtls1_start_timer(s);
			}
			ret = ssl3_send_server_hello(s);
			if (ret <= 0)
				goto end;
			if (s->internal->hit) {
				if (s->internal->tlsext_ticket_expected)
					S3I(s)->hs.state = SSL3_ST_SW_SESSION_TICKET_A;
				else
					S3I(s)->hs.state = SSL3_ST_SW_CHANGE_A;
			} else {
				S3I(s)->hs.state = SSL3_ST_SW_CERT_A;
			}
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_CERT_A:
		case SSL3_ST_SW_CERT_B:
			/* Check if it is anon DH or anon ECDH. */
			if (!(S3I(s)->hs.cipher->algorithm_auth &
			    SSL_aNULL)) {
				if (SSL_is_dtls(s))
					dtls1_start_timer(s);
				ret = ssl3_send_server_certificate(s);
				if (ret <= 0)
					goto end;
				if (s->internal->tlsext_status_expected)
					S3I(s)->hs.state = SSL3_ST_SW_CERT_STATUS_A;
				else
					S3I(s)->hs.state = SSL3_ST_SW_KEY_EXCH_A;
			} else {
				skip = 1;
				S3I(s)->hs.state = SSL3_ST_SW_KEY_EXCH_A;
			}
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_KEY_EXCH_A:
		case SSL3_ST_SW_KEY_EXCH_B:
			alg_k = S3I(s)->hs.cipher->algorithm_mkey;

			/*
			 * Only send if using a DH key exchange.
			 *
			 * For ECC ciphersuites, we send a ServerKeyExchange
			 * message only if the cipher suite is ECDHE. In other
			 * cases, the server certificate contains the server's
			 * public key for key exchange.
			 */
			if (alg_k & (SSL_kDHE|SSL_kECDHE)) {
				if (SSL_is_dtls(s))
					dtls1_start_timer(s);
				ret = ssl3_send_server_key_exchange(s);
				if (ret <= 0)
					goto end;
			} else
				skip = 1;

			S3I(s)->hs.state = SSL3_ST_SW_CERT_REQ_A;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_CERT_REQ_A:
		case SSL3_ST_SW_CERT_REQ_B:
			/*
			 * Determine whether or not we need to request a
			 * certificate.
			 *
			 * Do not request a certificate if:
			 *
			 * - We did not ask for it (SSL_VERIFY_PEER is unset).
			 *
			 * - SSL_VERIFY_CLIENT_ONCE is set and we are
			 *   renegotiating.
			 *
			 * - We are using an anonymous ciphersuites
			 *   (see section "Certificate request" in SSL 3 drafts
			 *   and in RFC 2246) ... except when the application
			 *   insists on verification (against the specs, but
			 *   s3_clnt.c accepts this for SSL 3).
			 */
			if (!(s->verify_mode & SSL_VERIFY_PEER) ||
			    ((s->session->peer != NULL) &&
			     (s->verify_mode & SSL_VERIFY_CLIENT_ONCE)) ||
			    ((S3I(s)->hs.cipher->algorithm_auth &
			     SSL_aNULL) && !(s->verify_mode &
			     SSL_VERIFY_FAIL_IF_NO_PEER_CERT))) {
				/* No cert request. */
				skip = 1;
				S3I(s)->tmp.cert_request = 0;
				S3I(s)->hs.state = SSL3_ST_SW_SRVR_DONE_A;

				if (!SSL_is_dtls(s))
					tls1_transcript_free(s);
			} else {
				S3I(s)->tmp.cert_request = 1;
				if (SSL_is_dtls(s))
					dtls1_start_timer(s);
				ret = ssl3_send_certificate_request(s);
				if (ret <= 0)
					goto end;
				S3I(s)->hs.state = SSL3_ST_SW_SRVR_DONE_A;
				s->internal->init_num = 0;
			}
			break;

		case SSL3_ST_SW_SRVR_DONE_A:
		case SSL3_ST_SW_SRVR_DONE_B:
			if (SSL_is_dtls(s))
				dtls1_start_timer(s);
			ret = ssl3_send_server_done(s);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.tls12.next_state = SSL3_ST_SR_CERT_A;
			S3I(s)->hs.state = SSL3_ST_SW_FLUSH;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_FLUSH:
			/*
			 * This code originally checked to see if
			 * any data was pending using BIO_CTRL_INFO
			 * and then flushed. This caused problems
			 * as documented in PR#1939. The proposed
			 * fix doesn't completely resolve this issue
			 * as buggy implementations of BIO_CTRL_PENDING
			 * still exist. So instead we just flush
			 * unconditionally.
			 */
			s->internal->rwstate = SSL_WRITING;
			if (BIO_flush(s->wbio) <= 0) {
				if (SSL_is_dtls(s)) {
					/* If the write error was fatal, stop trying. */
					if (!BIO_should_retry(s->wbio)) {
						s->internal->rwstate = SSL_NOTHING;
						S3I(s)->hs.state = S3I(s)->hs.tls12.next_state;
					}
				}
				ret = -1;
				goto end;
			}
			s->internal->rwstate = SSL_NOTHING;
			S3I(s)->hs.state = S3I(s)->hs.tls12.next_state;
			break;

		case SSL3_ST_SR_CERT_A:
		case SSL3_ST_SR_CERT_B:
			if (S3I(s)->tmp.cert_request) {
				ret = ssl3_get_client_certificate(s);
				if (ret <= 0)
					goto end;
			}
			s->internal->init_num = 0;
			S3I(s)->hs.state = SSL3_ST_SR_KEY_EXCH_A;
			break;

		case SSL3_ST_SR_KEY_EXCH_A:
		case SSL3_ST_SR_KEY_EXCH_B:
			ret = ssl3_get_client_key_exchange(s);
			if (ret <= 0)
				goto end;

			if (SSL_is_dtls(s)) {
				S3I(s)->hs.state = SSL3_ST_SR_CERT_VRFY_A;
				s->internal->init_num = 0;
			}

			alg_k = S3I(s)->hs.cipher->algorithm_mkey;
			if (ret == 2) {
				/*
				 * For the ECDH ciphersuites when
				 * the client sends its ECDH pub key in
				 * a certificate, the CertificateVerify
				 * message is not sent.
				 * Also for GOST ciphersuites when
				 * the client uses its key from the certificate
				 * for key exchange.
				 */
				S3I(s)->hs.state = SSL3_ST_SR_FINISHED_A;
				s->internal->init_num = 0;
			} else if (SSL_USE_SIGALGS(s) || (alg_k & SSL_kGOST)) {
				S3I(s)->hs.state = SSL3_ST_SR_CERT_VRFY_A;
				s->internal->init_num = 0;
				if (!s->session->peer)
					break;
				/*
				 * Freeze the transcript for use during client
				 * certificate verification.
				 */
				tls1_transcript_freeze(s);
			} else {
				S3I(s)->hs.state = SSL3_ST_SR_CERT_VRFY_A;
				s->internal->init_num = 0;

				tls1_transcript_free(s);

				/*
				 * We need to get hashes here so if there is
				 * a client cert, it can be verified.
				 */
				if (!tls1_transcript_hash_value(s,
				    S3I(s)->tmp.cert_verify_md,
				    sizeof(S3I(s)->tmp.cert_verify_md),
				    NULL)) {
					ret = -1;
					goto end;
				}
			}
			break;

		case SSL3_ST_SR_CERT_VRFY_A:
		case SSL3_ST_SR_CERT_VRFY_B:
			if (SSL_is_dtls(s))
				D1I(s)->change_cipher_spec_ok = 1;
			else
				s->s3->flags |= SSL3_FLAGS_CCS_OK;

			/* we should decide if we expected this one */
			ret = ssl3_get_cert_verify(s);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SR_FINISHED_A;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SR_FINISHED_A:
		case SSL3_ST_SR_FINISHED_B:
			if (SSL_is_dtls(s))
				D1I(s)->change_cipher_spec_ok = 1;
			else
				s->s3->flags |= SSL3_FLAGS_CCS_OK;
			ret = ssl3_get_finished(s, SSL3_ST_SR_FINISHED_A,
			    SSL3_ST_SR_FINISHED_B);
			if (ret <= 0)
				goto end;
			if (SSL_is_dtls(s))
				dtls1_stop_timer(s);
			if (s->internal->hit)
				S3I(s)->hs.state = SSL_ST_OK;
			else if (s->internal->tlsext_ticket_expected)
				S3I(s)->hs.state = SSL3_ST_SW_SESSION_TICKET_A;
			else
				S3I(s)->hs.state = SSL3_ST_SW_CHANGE_A;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_SESSION_TICKET_A:
		case SSL3_ST_SW_SESSION_TICKET_B:
			ret = ssl3_send_newsession_ticket(s);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SW_CHANGE_A;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_CERT_STATUS_A:
		case SSL3_ST_SW_CERT_STATUS_B:
			ret = ssl3_send_cert_status(s);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SW_KEY_EXCH_A;
			s->internal->init_num = 0;
			break;

		case SSL3_ST_SW_CHANGE_A:
		case SSL3_ST_SW_CHANGE_B:
			s->session->cipher = S3I(s)->hs.cipher;
			if (!tls1_setup_key_block(s)) {
				ret = -1;
				goto end;
			}

			ret = ssl3_send_change_cipher_spec(s,
			    SSL3_ST_SW_CHANGE_A, SSL3_ST_SW_CHANGE_B);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SW_FINISHED_A;
			s->internal->init_num = 0;

			if (!tls1_change_cipher_state(s,
			    SSL3_CHANGE_CIPHER_SERVER_WRITE)) {
				ret = -1;
				goto end;
			}

			if (SSL_is_dtls(s))
				dtls1_reset_seq_numbers(s, SSL3_CC_WRITE);
			break;

		case SSL3_ST_SW_FINISHED_A:
		case SSL3_ST_SW_FINISHED_B:
			ret = ssl3_send_finished(s,
			    SSL3_ST_SW_FINISHED_A, SSL3_ST_SW_FINISHED_B,
			    TLS_MD_SERVER_FINISH_CONST,
			    TLS_MD_SERVER_FINISH_CONST_SIZE);
			if (ret <= 0)
				goto end;
			S3I(s)->hs.state = SSL3_ST_SW_FLUSH;
			if (s->internal->hit) {
				S3I(s)->hs.tls12.next_state = SSL3_ST_SR_FINISHED_A;
				tls1_transcript_free(s);
			} else
				S3I(s)->hs.tls12.next_state = SSL_ST_OK;
			s->internal->init_num = 0;
			break;

		case SSL_ST_OK:
			/* clean a few things up */
			tls1_cleanup_key_block(s);

			if (S3I(s)->handshake_transcript != NULL) {
				SSLerror(s, ERR_R_INTERNAL_ERROR);
				ret = -1;
				goto end;
			}

			if (!SSL_is_dtls(s))
				ssl3_release_init_buffer(s);

			/* remove buffering on output */
			ssl_free_wbio_buffer(s);

			s->internal->init_num = 0;

			/* Skipped if we just sent a HelloRequest. */
			if (s->internal->renegotiate == 2) {
				s->internal->renegotiate = 0;
				s->internal->new_session = 0;

				ssl_update_cache(s, SSL_SESS_CACHE_SERVER);

				s->ctx->internal->stats.sess_accept_good++;
				/* s->server=1; */
				s->internal->handshake_func = ssl3_accept;

				if (cb != NULL)
					cb(s, SSL_CB_HANDSHAKE_DONE, 1);
			}

			ret = 1;

			if (SSL_is_dtls(s)) {
				/* Done handshaking, next message is client hello. */
				D1I(s)->handshake_read_seq = 0;
				/* Next message is server hello. */
				D1I(s)->handshake_write_seq = 0;
				D1I(s)->next_handshake_write_seq = 0;
			}
			goto end;
			/* break; */

		default:
			SSLerror(s, SSL_R_UNKNOWN_STATE);
			ret = -1;
			goto end;
			/* break; */
		}

		if (!S3I(s)->tmp.reuse_message && !skip) {
			if (s->internal->debug) {
				if ((ret = BIO_flush(s->wbio)) <= 0)
					goto end;
			}


			if ((cb != NULL) && (S3I(s)->hs.state != state)) {
				new_state = S3I(s)->hs.state;
				S3I(s)->hs.state = state;
				cb(s, SSL_CB_ACCEPT_LOOP, 1);
				S3I(s)->hs.state = new_state;
			}
		}
		skip = 0;
	}
 end:
	/* BIO_flush(s->wbio); */
	s->internal->in_handshake--;
	if (cb != NULL)
		cb(s, SSL_CB_ACCEPT_EXIT, ret);

	return (ret);
}

int
ssl3_send_hello_request(SSL *s)
{
	CBB cbb, hello;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_HELLO_REQ_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &hello,
		    SSL3_MT_HELLO_REQUEST))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_HELLO_REQ_B;
	}

	/* SSL3_ST_SW_HELLO_REQ_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

int
ssl3_get_client_hello(SSL *s)
{
	CBS cbs, client_random, session_id, cookie, cipher_suites;
	CBS compression_methods;
	uint16_t client_version;
	uint8_t comp_method;
	int comp_null;
	int i, j, ok, al, ret = -1, cookie_valid = 0;
	long n;
	unsigned long id;
	SSL_CIPHER *c;
	STACK_OF(SSL_CIPHER) *ciphers = NULL;
	unsigned long alg_k;
	const SSL_METHOD *method;
	uint16_t shared_version;

	/*
	 * We do this so that we will respond with our native type.
	 * If we are TLSv1 and we get SSLv3, we will respond with TLSv1,
	 * This down switching should be handled by a different method.
	 * If we are SSLv3, we will respond with SSLv3, even if prompted with
	 * TLSv1.
	 */
	if (S3I(s)->hs.state == SSL3_ST_SR_CLNT_HELLO_A) {
		S3I(s)->hs.state = SSL3_ST_SR_CLNT_HELLO_B;
	}

	s->internal->first_packet = 1;
	n = ssl3_get_message(s, SSL3_ST_SR_CLNT_HELLO_B,
	    SSL3_ST_SR_CLNT_HELLO_C, SSL3_MT_CLIENT_HELLO,
	    SSL3_RT_MAX_PLAIN_LENGTH, &ok);
	if (!ok)
		return ((int)n);
	s->internal->first_packet = 0;

	if (n < 0)
		goto err;

	CBS_init(&cbs, s->internal->init_msg, n);

	/* Parse client hello up until the extensions (if any). */
	if (!CBS_get_u16(&cbs, &client_version))
		goto decode_err;
	if (!CBS_get_bytes(&cbs, &client_random, SSL3_RANDOM_SIZE))
		goto decode_err;
	if (!CBS_get_u8_length_prefixed(&cbs, &session_id))
		goto decode_err;
	if (CBS_len(&session_id) > SSL3_SESSION_ID_SIZE) {
		al = SSL_AD_ILLEGAL_PARAMETER;
		SSLerror(s, SSL_R_SSL3_SESSION_ID_TOO_LONG);
		goto fatal_err;
	}
	if (SSL_is_dtls(s)) {
		if (!CBS_get_u8_length_prefixed(&cbs, &cookie))
			goto decode_err;
	}
	if (!CBS_get_u16_length_prefixed(&cbs, &cipher_suites))
		goto decode_err;
	if (!CBS_get_u8_length_prefixed(&cbs, &compression_methods))
		goto decode_err;

	/*
	 * Use version from inside client hello, not from record header.
	 * (may differ: see RFC 2246, Appendix E, second paragraph)
	 */
	if (!ssl_max_shared_version(s, client_version, &shared_version)) {
		if ((s->client_version >> 8) == SSL3_VERSION_MAJOR &&
		    !tls12_record_layer_write_protected(s->internal->rl)) {
			/*
			 * Similar to ssl3_get_record, send alert using remote
			 * version number.
			 */
			s->version = s->client_version;
		}
		SSLerror(s, SSL_R_WRONG_VERSION_NUMBER);
		al = SSL_AD_PROTOCOL_VERSION;
		goto fatal_err;
	}
	s->client_version = client_version;
	s->version = shared_version;

	S3I(s)->hs.negotiated_tls_version = ssl_tls_version(shared_version);
	if (S3I(s)->hs.negotiated_tls_version == 0) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	if ((method = ssl_get_method(shared_version)) == NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}
	s->method = method;

	/*
	 * If we require cookies (DTLS) and this ClientHello does not contain
	 * one, just return since we do not want to allocate any memory yet.
	 * So check cookie length...
	 */
	if (SSL_is_dtls(s)) {
		if (SSL_get_options(s) & SSL_OP_COOKIE_EXCHANGE) {
			if (CBS_len(&cookie) == 0)
				return (1);
		}
	}

	if (!CBS_write_bytes(&client_random, s->s3->client_random,
	    sizeof(s->s3->client_random), NULL))
		goto err;

	s->internal->hit = 0;

	/*
	 * Versions before 0.9.7 always allow clients to resume sessions in
	 * renegotiation. 0.9.7 and later allow this by default, but optionally
	 * ignore resumption requests with flag
	 * SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION (it's a new flag
	 * rather than a change to default behavior so that applications
	 * relying on this for security won't even compile against older
	 * library versions).
	 *
	 * 1.0.1 and later also have a function SSL_renegotiate_abbreviated()
	 * to request renegotiation but not a new session (s->internal->new_session
	 * remains unset): for servers, this essentially just means that the
	 * SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION setting will be
	 * ignored.
	 */
	if ((s->internal->new_session && (s->internal->options &
	    SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION))) {
		if (!ssl_get_new_session(s, 1))
			goto err;
	} else {
		CBS ext_block;

		CBS_dup(&cbs, &ext_block);

		i = ssl_get_prev_session(s, &session_id, &ext_block, &al);
		if (i == 1) { /* previous session */
			s->internal->hit = 1;
		} else if (i == -1)
			goto fatal_err;
		else {
			/* i == 0 */
			if (!ssl_get_new_session(s, 1))
				goto err;
		}
	}

	if (SSL_is_dtls(s)) {
		/*
		 * The ClientHello may contain a cookie even if the HelloVerify
		 * message has not been sent - make sure that it does not cause
		 * an overflow.
		 */
		if (CBS_len(&cookie) > sizeof(D1I(s)->rcvd_cookie)) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_COOKIE_MISMATCH);
			goto fatal_err;
		}

		/* Verify the cookie if appropriate option is set. */
		if ((SSL_get_options(s) & SSL_OP_COOKIE_EXCHANGE) &&
		    CBS_len(&cookie) > 0) {
			size_t cookie_len;

			/* XXX - rcvd_cookie seems to only be used here... */
			if (!CBS_write_bytes(&cookie, D1I(s)->rcvd_cookie,
			    sizeof(D1I(s)->rcvd_cookie), &cookie_len))
				goto err;

			if (s->ctx->internal->app_verify_cookie_cb != NULL) {
				if (s->ctx->internal->app_verify_cookie_cb(s,
				    D1I(s)->rcvd_cookie, cookie_len) == 0) {
					al = SSL_AD_HANDSHAKE_FAILURE;
					SSLerror(s, SSL_R_COOKIE_MISMATCH);
					goto fatal_err;
				}
				/* else cookie verification succeeded */
			/* XXX - can d1->cookie_len > sizeof(rcvd_cookie) ? */
			} else if (timingsafe_memcmp(D1I(s)->rcvd_cookie,
			    D1I(s)->cookie, D1I(s)->cookie_len) != 0) {
				/* default verification */
				al = SSL_AD_HANDSHAKE_FAILURE;
				SSLerror(s, SSL_R_COOKIE_MISMATCH);
				goto fatal_err;
			}
			cookie_valid = 1;
		}
	}

	/* XXX - This logic seems wrong... */
	if (CBS_len(&cipher_suites) == 0 && CBS_len(&session_id) != 0) {
		/* we need a cipher if we are not resuming a session */
		al = SSL_AD_ILLEGAL_PARAMETER;
		SSLerror(s, SSL_R_NO_CIPHERS_SPECIFIED);
		goto fatal_err;
	}

	if (CBS_len(&cipher_suites) > 0) {
		if ((ciphers = ssl_bytes_to_cipher_list(s,
		    &cipher_suites)) == NULL)
			goto err;
	}

	/* If it is a hit, check that the cipher is in the list */
	/* XXX - CBS_len(&cipher_suites) will always be zero here... */
	if (s->internal->hit && CBS_len(&cipher_suites) > 0) {
		j = 0;
		id = s->session->cipher->id;

		for (i = 0; i < sk_SSL_CIPHER_num(ciphers); i++) {
			c = sk_SSL_CIPHER_value(ciphers, i);
			if (c->id == id) {
				j = 1;
				break;
			}
		}
		if (j == 0) {
			/*
			 * We need to have the cipher in the cipher
			 * list if we are asked to reuse it
			 */
			al = SSL_AD_ILLEGAL_PARAMETER;
			SSLerror(s, SSL_R_REQUIRED_CIPHER_MISSING);
			goto fatal_err;
		}
	}

	comp_null = 0;
	while (CBS_len(&compression_methods) > 0) {
		if (!CBS_get_u8(&compression_methods, &comp_method))
			goto decode_err;
		if (comp_method == 0)
			comp_null = 1;
	}
	if (comp_null == 0) {
		al = SSL_AD_DECODE_ERROR;
		SSLerror(s, SSL_R_NO_COMPRESSION_SPECIFIED);
		goto fatal_err;
	}

	if (!tlsext_server_parse(s, SSL_TLSEXT_MSG_CH, &cbs, &al)) {
		SSLerror(s, SSL_R_PARSE_TLSEXT);
		goto fatal_err;
	}

	if (!S3I(s)->renegotiate_seen && s->internal->renegotiate) {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_UNSAFE_LEGACY_RENEGOTIATION_DISABLED);
		goto fatal_err;
	}

	if (ssl_check_clienthello_tlsext_early(s) <= 0) {
		SSLerror(s, SSL_R_CLIENTHELLO_TLSEXT);
		goto err;
	}

	/*
	 * Check if we want to use external pre-shared secret for this
	 * handshake for not reused session only. We need to generate
	 * server_random before calling tls_session_secret_cb in order to allow
	 * SessionTicket processing to use it in key derivation.
	 */
	arc4random_buf(s->s3->server_random, SSL3_RANDOM_SIZE);

	if (S3I(s)->hs.our_max_tls_version >= TLS1_2_VERSION &&
	    S3I(s)->hs.negotiated_tls_version < S3I(s)->hs.our_max_tls_version) {
		/*
		 * RFC 8446 section 4.1.3. If we are downgrading from TLS 1.3
		 * we must set the last 8 bytes of the server random to magical
		 * values to indicate we meant to downgrade.  For TLS 1.2 it is
		 * recommended that we do the same.
		 */
		size_t index = SSL3_RANDOM_SIZE - sizeof(tls13_downgrade_12);
		uint8_t *magic = &s->s3->server_random[index];
		if (S3I(s)->hs.negotiated_tls_version == TLS1_2_VERSION) {
			/* Indicate we chose to downgrade to 1.2. */
			memcpy(magic, tls13_downgrade_12,
			    sizeof(tls13_downgrade_12));
		} else {
			/* Indicate we chose to downgrade to 1.1 or lower */
			memcpy(magic, tls13_downgrade_11,
			    sizeof(tls13_downgrade_11));
		}
	}

	if (!s->internal->hit && s->internal->tls_session_secret_cb) {
		SSL_CIPHER *pref_cipher = NULL;

		s->session->master_key_length = sizeof(s->session->master_key);
		if (s->internal->tls_session_secret_cb(s, s->session->master_key,
		    &s->session->master_key_length, ciphers, &pref_cipher,
		    s->internal->tls_session_secret_cb_arg)) {
			s->internal->hit = 1;
			s->session->ciphers = ciphers;
			s->session->verify_result = X509_V_OK;

			ciphers = NULL;

			/* check if some cipher was preferred by call back */
			pref_cipher = pref_cipher ? pref_cipher :
			    ssl3_choose_cipher(s, s->session->ciphers,
			    SSL_get_ciphers(s));
			if (pref_cipher == NULL) {
				al = SSL_AD_HANDSHAKE_FAILURE;
				SSLerror(s, SSL_R_NO_SHARED_CIPHER);
				goto fatal_err;
			}

			s->session->cipher = pref_cipher;

			sk_SSL_CIPHER_free(s->cipher_list);
			s->cipher_list = sk_SSL_CIPHER_dup(s->session->ciphers);
		}
	}

	/*
	 * Given s->session->ciphers and SSL_get_ciphers, we must
	 * pick a cipher
	 */

	if (!s->internal->hit) {
		sk_SSL_CIPHER_free(s->session->ciphers);
		s->session->ciphers = ciphers;
		if (ciphers == NULL) {
			al = SSL_AD_ILLEGAL_PARAMETER;
			SSLerror(s, SSL_R_NO_CIPHERS_PASSED);
			goto fatal_err;
		}
		ciphers = NULL;
		c = ssl3_choose_cipher(s, s->session->ciphers,
		SSL_get_ciphers(s));

		if (c == NULL) {
			al = SSL_AD_HANDSHAKE_FAILURE;
			SSLerror(s, SSL_R_NO_SHARED_CIPHER);
			goto fatal_err;
		}
		S3I(s)->hs.cipher = c;
	} else {
		S3I(s)->hs.cipher = s->session->cipher;
	}

	if (!tls1_transcript_hash_init(s))
		goto err;

	alg_k = S3I(s)->hs.cipher->algorithm_mkey;
	if (!(SSL_USE_SIGALGS(s) || (alg_k & SSL_kGOST)) ||
	    !(s->verify_mode & SSL_VERIFY_PEER))
		tls1_transcript_free(s);

	/*
	 * We now have the following setup.
	 * client_random
	 * cipher_list		- our prefered list of ciphers
	 * ciphers		- the clients prefered list of ciphers
	 * compression		- basically ignored right now
	 * ssl version is set	- sslv3
	 * s->session		- The ssl session has been setup.
	 * s->internal->hit		- session reuse flag
	 * s->hs.cipher	- the new cipher to use.
	 */

	/* Handles TLS extensions that we couldn't check earlier */
	if (ssl_check_clienthello_tlsext_late(s) <= 0) {
		SSLerror(s, SSL_R_CLIENTHELLO_TLSEXT);
		goto err;
	}

	ret = cookie_valid ? 2 : 1;

	if (0) {
 decode_err:
		al = SSL_AD_DECODE_ERROR;
		SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
 fatal_err:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
	}
 err:
	sk_SSL_CIPHER_free(ciphers);

	return (ret);
}

int
ssl3_send_dtls_hello_verify_request(SSL *s)
{
	CBB cbb, verify, cookie;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == DTLS1_ST_SW_HELLO_VERIFY_REQUEST_A) {
		if (s->ctx->internal->app_gen_cookie_cb == NULL ||
		    s->ctx->internal->app_gen_cookie_cb(s, D1I(s)->cookie,
			&(D1I(s)->cookie_len)) == 0) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			return 0;
		}

		/*
		 * Per RFC 6347 section 4.2.1, the HelloVerifyRequest should
		 * always contain DTLSv1.0 regardless of the version that is
		 * going to be negotiated.
		 */
		if (!ssl3_handshake_msg_start(s, &cbb, &verify,
		    DTLS1_MT_HELLO_VERIFY_REQUEST))
			goto err;
		if (!CBB_add_u16(&verify, DTLS1_VERSION))
			goto err;
		if (!CBB_add_u8_length_prefixed(&verify, &cookie))
			goto err;
		if (!CBB_add_bytes(&cookie, D1I(s)->cookie, D1I(s)->cookie_len))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B;
	}

	/* S3I(s)->hs.state = DTLS1_ST_SW_HELLO_VERIFY_REQUEST_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

int
ssl3_send_server_hello(SSL *s)
{
	CBB cbb, server_hello, session_id;
	size_t sl;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_SRVR_HELLO_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &server_hello,
		    SSL3_MT_SERVER_HELLO))
			goto err;

		if (!CBB_add_u16(&server_hello, s->version))
			goto err;
		if (!CBB_add_bytes(&server_hello, s->s3->server_random,
		    sizeof(s->s3->server_random)))
			goto err;

		/*
		 * There are several cases for the session ID to send
		 * back in the server hello:
		 *
		 * - For session reuse from the session cache,
		 *   we send back the old session ID.
		 * - If stateless session reuse (using a session ticket)
		 *   is successful, we send back the client's "session ID"
		 *   (which doesn't actually identify the session).
		 * - If it is a new session, we send back the new
		 *   session ID.
		 * - However, if we want the new session to be single-use,
		 *   we send back a 0-length session ID.
		 *
		 * s->internal->hit is non-zero in either case of session reuse,
		 * so the following won't overwrite an ID that we're supposed
		 * to send back.
		 */
		if (!(s->ctx->internal->session_cache_mode & SSL_SESS_CACHE_SERVER)
		    && !s->internal->hit)
			s->session->session_id_length = 0;

		sl = s->session->session_id_length;
		if (sl > sizeof(s->session->session_id)) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			goto err;
		}
		if (!CBB_add_u8_length_prefixed(&server_hello, &session_id))
			goto err;
		if (!CBB_add_bytes(&session_id, s->session->session_id, sl))
			goto err;

		/* Cipher suite. */
		if (!CBB_add_u16(&server_hello,
		    ssl3_cipher_get_value(S3I(s)->hs.cipher)))
			goto err;

		/* Compression method (null). */
		if (!CBB_add_u8(&server_hello, 0))
			goto err;

		/* TLS extensions */
		if (!tlsext_server_build(s, SSL_TLSEXT_MSG_SH, &server_hello)) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			goto err;
		}

		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;
	}

	/* SSL3_ST_SW_SRVR_HELLO_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

int
ssl3_send_server_done(SSL *s)
{
	CBB cbb, done;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_SRVR_DONE_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &done,
		    SSL3_MT_SERVER_DONE))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_SRVR_DONE_B;
	}

	/* SSL3_ST_SW_SRVR_DONE_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

static int
ssl3_send_server_kex_dhe(SSL *s, CBB *cbb)
{
	CBB dh_p, dh_g, dh_Ys;
	DH *dh = NULL, *dhp;
	unsigned char *data;
	int al;

	if (s->cert->dh_tmp_auto != 0) {
		if ((dhp = ssl_get_auto_dh(s)) == NULL) {
			al = SSL_AD_INTERNAL_ERROR;
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			goto fatal_err;
		}
	} else
		dhp = s->cert->dh_tmp;

	if (dhp == NULL && s->cert->dh_tmp_cb != NULL)
		dhp = s->cert->dh_tmp_cb(s, 0,
		    SSL_C_PKEYLENGTH(S3I(s)->hs.cipher));

	if (dhp == NULL) {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_MISSING_TMP_DH_KEY);
		goto fatal_err;
	}

	if (S3I(s)->tmp.dh != NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	if (s->cert->dh_tmp_auto != 0) {
		dh = dhp;
	} else if ((dh = DHparams_dup(dhp)) == NULL) {
		SSLerror(s, ERR_R_DH_LIB);
		goto err;
	}
	S3I(s)->tmp.dh = dh;
	if (!DH_generate_key(dh)) {
		SSLerror(s, ERR_R_DH_LIB);
		goto err;
	}

	/*
	 * Serialize the DH parameters and public key.
	 */
	if (!CBB_add_u16_length_prefixed(cbb, &dh_p))
		goto err;
	if (!CBB_add_space(&dh_p, &data, BN_num_bytes(dh->p)))
		goto err;
	BN_bn2bin(dh->p, data);

	if (!CBB_add_u16_length_prefixed(cbb, &dh_g))
		goto err;
	if (!CBB_add_space(&dh_g, &data, BN_num_bytes(dh->g)))
		goto err;
	BN_bn2bin(dh->g, data);

	if (!CBB_add_u16_length_prefixed(cbb, &dh_Ys))
		goto err;
	if (!CBB_add_space(&dh_Ys, &data, BN_num_bytes(dh->pub_key)))
		goto err;
	BN_bn2bin(dh->pub_key, data);

	if (!CBB_flush(cbb))
		goto err;

	return (1);

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (-1);
}

static int
ssl3_send_server_kex_ecdhe_ecp(SSL *s, int nid, CBB *cbb)
{
	uint16_t curve_id;
	EC_KEY *ecdh;
	CBB ecpoint;
	int al;

	/*
	 * Only named curves are supported in ECDH ephemeral key exchanges.
	 * For supported named curves, curve_id is non-zero.
	 */
	if ((curve_id = tls1_ec_nid2curve_id(nid)) == 0) {
		SSLerror(s, SSL_R_UNSUPPORTED_ELLIPTIC_CURVE);
		goto err;
	}

	if (S3I(s)->tmp.ecdh != NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	if ((S3I(s)->tmp.ecdh = EC_KEY_new()) == NULL) {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_MISSING_TMP_ECDH_KEY);
		goto fatal_err;
	}
	S3I(s)->tmp.ecdh_nid = nid;
	ecdh = S3I(s)->tmp.ecdh;

	if (!ssl_kex_generate_ecdhe_ecp(ecdh, nid))
		goto err;

	/*
	 * Encode the public key.
	 *
	 * Only named curves are supported in ECDH ephemeral key exchanges.
	 * In this case the ServerKeyExchange message has:
	 * [1 byte CurveType], [2 byte CurveName]
	 * [1 byte length of encoded point], followed by
	 * the actual encoded point itself.
	 */
	if (!CBB_add_u8(cbb, NAMED_CURVE_TYPE))
		goto err;
	if (!CBB_add_u16(cbb, curve_id))
		goto err;
	if (!CBB_add_u8_length_prefixed(cbb, &ecpoint))
		goto err;
	if (!ssl_kex_public_ecdhe_ecp(ecdh, &ecpoint))
		goto err;
	if (!CBB_flush(cbb))
		goto err;

	return (1);

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (-1);
}

static int
ssl3_send_server_kex_ecdhe_ecx(SSL *s, int nid, CBB *cbb)
{
	uint8_t *public_key = NULL, *private_key = NULL;
	uint16_t curve_id;
	CBB ecpoint;
	int ret = -1;

	/* Generate an X25519 key pair. */
	if (S3I(s)->tmp.x25519 != NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}
	if ((private_key = malloc(X25519_KEY_LENGTH)) == NULL)
		goto err;
	if ((public_key = malloc(X25519_KEY_LENGTH)) == NULL)
		goto err;
	X25519_keypair(public_key, private_key);

	/* Serialize public key. */
	if ((curve_id = tls1_ec_nid2curve_id(nid)) == 0) {
		SSLerror(s, SSL_R_UNSUPPORTED_ELLIPTIC_CURVE);
		goto err;
	}

	if (!CBB_add_u8(cbb, NAMED_CURVE_TYPE))
		goto err;
	if (!CBB_add_u16(cbb, curve_id))
		goto err;
	if (!CBB_add_u8_length_prefixed(cbb, &ecpoint))
		goto err;
	if (!CBB_add_bytes(&ecpoint, public_key, X25519_KEY_LENGTH))
		goto err;
	if (!CBB_flush(cbb))
		goto err;

	S3I(s)->tmp.x25519 = private_key;
	private_key = NULL;
	ret = 1;

 err:
	free(public_key);
	freezero(private_key, X25519_KEY_LENGTH);

	return (ret);
}

static int
ssl3_send_server_kex_ecdhe(SSL *s, CBB *cbb)
{
	int nid;

	nid = tls1_get_shared_curve(s);

	if (nid == NID_X25519)
		return ssl3_send_server_kex_ecdhe_ecx(s, nid, cbb);

	return ssl3_send_server_kex_ecdhe_ecp(s, nid, cbb);
}

int
ssl3_send_server_key_exchange(SSL *s)
{
	CBB cbb, cbb_params, cbb_signature, server_kex;
	const struct ssl_sigalg *sigalg = NULL;
	unsigned char *signature = NULL;
	size_t signature_len = 0;
	unsigned char *params = NULL;
	size_t params_len;
	const EVP_MD *md = NULL;
	unsigned long type;
	EVP_MD_CTX md_ctx;
	EVP_PKEY_CTX *pctx;
	EVP_PKEY *pkey;
	int al;

	memset(&cbb, 0, sizeof(cbb));
	memset(&cbb_params, 0, sizeof(cbb_params));

	EVP_MD_CTX_init(&md_ctx);

	if (S3I(s)->hs.state == SSL3_ST_SW_KEY_EXCH_A) {

		if (!ssl3_handshake_msg_start(s, &cbb, &server_kex,
		    SSL3_MT_SERVER_KEY_EXCHANGE))
			goto err;

		if (!CBB_init(&cbb_params, 0))
			goto err;

		type = S3I(s)->hs.cipher->algorithm_mkey;
		if (type & SSL_kDHE) {
			if (ssl3_send_server_kex_dhe(s, &cbb_params) != 1)
				goto err;
		} else if (type & SSL_kECDHE) {
			if (ssl3_send_server_kex_ecdhe(s, &cbb_params) != 1)
				goto err;
		} else {
			al = SSL_AD_HANDSHAKE_FAILURE;
			SSLerror(s, SSL_R_UNKNOWN_KEY_EXCHANGE_TYPE);
			goto fatal_err;
		}

		if (!CBB_finish(&cbb_params, &params, &params_len))
			goto err;

		if (!CBB_add_bytes(&server_kex, params, params_len))
			goto err;

		/* Add signature unless anonymous. */
		if (!(S3I(s)->hs.cipher->algorithm_auth & SSL_aNULL)) {
			if ((pkey = ssl_get_sign_pkey(s, S3I(s)->hs.cipher,
			    &md, &sigalg)) == NULL) {
				al = SSL_AD_DECODE_ERROR;
				goto fatal_err;
			}

			/* Send signature algorithm. */
			if (SSL_USE_SIGALGS(s)) {
				if (!CBB_add_u16(&server_kex, sigalg->value)) {
					al = SSL_AD_INTERNAL_ERROR;
					SSLerror(s, ERR_R_INTERNAL_ERROR);
					goto fatal_err;
				}
			}

			if (!EVP_DigestSignInit(&md_ctx, &pctx, md, NULL, pkey)) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if ((sigalg->flags & SIGALG_FLAG_RSA_PSS) &&
			    (!EVP_PKEY_CTX_set_rsa_padding(pctx,
			    RSA_PKCS1_PSS_PADDING) ||
			    !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1))) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if (!EVP_DigestSignUpdate(&md_ctx, s->s3->client_random,
			    SSL3_RANDOM_SIZE)) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if (!EVP_DigestSignUpdate(&md_ctx, s->s3->server_random,
			    SSL3_RANDOM_SIZE)) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if (!EVP_DigestSignUpdate(&md_ctx, params, params_len)) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if (!EVP_DigestSignFinal(&md_ctx, NULL, &signature_len) ||
			    !signature_len) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}
			if ((signature = calloc(1, signature_len)) == NULL) {
				SSLerror(s, ERR_R_MALLOC_FAILURE);
				goto err;
			}
			if (!EVP_DigestSignFinal(&md_ctx, signature, &signature_len)) {
				SSLerror(s, ERR_R_EVP_LIB);
				goto err;
			}

			if (!CBB_add_u16_length_prefixed(&server_kex,
			    &cbb_signature))
				goto err;
			if (!CBB_add_bytes(&cbb_signature, signature,
			    signature_len))
				goto err;
		}

		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_KEY_EXCH_B;
	}

	EVP_MD_CTX_cleanup(&md_ctx);
	free(params);
	free(signature);

	return (ssl3_handshake_write(s));

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	CBB_cleanup(&cbb_params);
	CBB_cleanup(&cbb);
	EVP_MD_CTX_cleanup(&md_ctx);
	free(params);
	free(signature);

	return (-1);
}

int
ssl3_send_certificate_request(SSL *s)
{
	CBB cbb, cert_request, cert_types, sigalgs, cert_auth, dn;
	STACK_OF(X509_NAME) *sk = NULL;
	X509_NAME *name;
	int i;

	/*
	 * Certificate Request - RFC 5246 section 7.4.4.
	 */

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_CERT_REQ_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &cert_request,
		    SSL3_MT_CERTIFICATE_REQUEST))
			goto err;

		if (!CBB_add_u8_length_prefixed(&cert_request, &cert_types))
			goto err;
		if (!ssl3_get_req_cert_types(s, &cert_types))
			goto err;

		if (SSL_USE_SIGALGS(s)) {
			if (!CBB_add_u16_length_prefixed(&cert_request, &sigalgs))
				goto err;
			if (!ssl_sigalgs_build(&sigalgs, tls12_sigalgs, tls12_sigalgs_len))
				goto err;
		}

		if (!CBB_add_u16_length_prefixed(&cert_request, &cert_auth))
			goto err;

		sk = SSL_get_client_CA_list(s);
		for (i = 0; i < sk_X509_NAME_num(sk); i++) {
			unsigned char *name_data;
			size_t name_len;

			name = sk_X509_NAME_value(sk, i);
			name_len = i2d_X509_NAME(name, NULL);

			if (!CBB_add_u16_length_prefixed(&cert_auth, &dn))
				goto err;
			if (!CBB_add_space(&dn, &name_data, name_len))
				goto err;
			if (i2d_X509_NAME(name, &name_data) != name_len)
				goto err;
		}

		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_CERT_REQ_B;
	}

	/* SSL3_ST_SW_CERT_REQ_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

static int
ssl3_get_client_kex_rsa(SSL *s, CBS *cbs)
{
	unsigned char fakekey[SSL_MAX_MASTER_KEY_LENGTH];
	unsigned char *pms = NULL;
	unsigned char *p;
	size_t pms_len = 0;
	EVP_PKEY *pkey = NULL;
	RSA *rsa = NULL;
	CBS enc_pms;
	int decrypt_len;
	int al = -1;

	arc4random_buf(fakekey, sizeof(fakekey));

	/* XXX - peer max protocol version. */
	fakekey[0] = s->client_version >> 8;
	fakekey[1] = s->client_version & 0xff;

	pkey = s->cert->pkeys[SSL_PKEY_RSA].privatekey;
	if ((pkey == NULL) || (pkey->type != EVP_PKEY_RSA) ||
	    (pkey->pkey.rsa == NULL)) {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_MISSING_RSA_CERTIFICATE);
		goto fatal_err;
	}
	rsa = pkey->pkey.rsa;

	pms_len = RSA_size(rsa);
	if (pms_len < SSL_MAX_MASTER_KEY_LENGTH)
		goto err;
	if ((pms = malloc(pms_len)) == NULL)
		goto err;
	p = pms;

	if (!CBS_get_u16_length_prefixed(cbs, &enc_pms))
		goto decode_err;
	if (CBS_len(cbs) != 0 || CBS_len(&enc_pms) != RSA_size(rsa)) {
		SSLerror(s, SSL_R_TLS_RSA_ENCRYPTED_VALUE_LENGTH_IS_WRONG);
		goto err;
	}

	decrypt_len = RSA_private_decrypt(CBS_len(&enc_pms), CBS_data(&enc_pms),
	    pms, rsa, RSA_PKCS1_PADDING);

	ERR_clear_error();

	if (decrypt_len != SSL_MAX_MASTER_KEY_LENGTH) {
		al = SSL_AD_DECODE_ERROR;
		/* SSLerror(s, SSL_R_BAD_RSA_DECRYPT); */
	}

	/* XXX - peer max version. */
	if ((al == -1) && !((pms[0] == (s->client_version >> 8)) &&
	    (pms[1] == (s->client_version & 0xff)))) {
		/*
		 * The premaster secret must contain the same version number
		 * as the ClientHello to detect version rollback attacks
		 * (strangely, the protocol does not offer such protection for
		 * DH ciphersuites).
		 *
		 * The Klima-Pokorny-Rosa extension of Bleichenbacher's attack
		 * (http://eprint.iacr.org/2003/052/) exploits the version
		 * number check as a "bad version oracle" -- an alert would
		 * reveal that the plaintext corresponding to some ciphertext
		 * made up by the adversary is properly formatted except that
		 * the version number is wrong. To avoid such attacks, we should
		 * treat this just like any other decryption error.
		 */
		al = SSL_AD_DECODE_ERROR;
		/* SSLerror(s, SSL_R_BAD_PROTOCOL_VERSION_NUMBER); */
	}

	if (al != -1) {
		/*
		 * Some decryption failure -- use random value instead
		 * as countermeasure against Bleichenbacher's attack
		 * on PKCS #1 v1.5 RSA padding (see RFC 2246,
		 * section 7.4.7.1).
		 */
		p = fakekey;
	}

	s->session->master_key_length =
	    tls1_generate_master_secret(s,
		s->session->master_key, p, SSL_MAX_MASTER_KEY_LENGTH);

	freezero(pms, pms_len);

	return (1);

 decode_err:
	al = SSL_AD_DECODE_ERROR;
	SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	freezero(pms, pms_len);

	return (-1);
}

static int
ssl3_get_client_kex_dhe(SSL *s, CBS *cbs)
{
	int key_size = 0;
	int key_is_invalid, key_len, al;
	unsigned char *key = NULL;
	BIGNUM *bn = NULL;
	CBS dh_Yc;
	DH *dh;

	if (!CBS_get_u16_length_prefixed(cbs, &dh_Yc))
		goto decode_err;
	if (CBS_len(cbs) != 0)
		goto decode_err;

	if (S3I(s)->tmp.dh == NULL) {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_MISSING_TMP_DH_KEY);
		goto fatal_err;
	}
	dh = S3I(s)->tmp.dh;

	if ((bn = BN_bin2bn(CBS_data(&dh_Yc), CBS_len(&dh_Yc), NULL)) == NULL) {
		SSLerror(s, SSL_R_BN_LIB);
		goto err;
	}

	if ((key_size = DH_size(dh)) <= 0) {
		SSLerror(s, ERR_R_DH_LIB);
		goto err;
	}
	if ((key = malloc(key_size)) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	if (!DH_check_pub_key(dh, bn, &key_is_invalid)) {
		al = SSL_AD_INTERNAL_ERROR;
		SSLerror(s, ERR_R_DH_LIB);
		goto fatal_err;
	}
	if (key_is_invalid) {
		al = SSL_AD_ILLEGAL_PARAMETER;
		SSLerror(s, ERR_R_DH_LIB);
		goto fatal_err;
	}
	if ((key_len = DH_compute_key(key, bn, dh)) <= 0) {
		al = SSL_AD_INTERNAL_ERROR;
		SSLerror(s, ERR_R_DH_LIB);
		goto fatal_err;
	}

	s->session->master_key_length = tls1_generate_master_secret(s,
	    s->session->master_key, key, key_len);

	DH_free(S3I(s)->tmp.dh);
	S3I(s)->tmp.dh = NULL;

	freezero(key, key_size);
	BN_clear_free(bn);

	return (1);

 decode_err:
	al = SSL_AD_DECODE_ERROR;
	SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	freezero(key, key_size);
	BN_clear_free(bn);

	return (-1);
}

static int
ssl3_get_client_kex_ecdhe_ecp(SSL *s, CBS *cbs)
{
	uint8_t *key = NULL;
	size_t key_len = 0;
	EC_KEY *ecdh_peer = NULL;
	EC_KEY *ecdh;
	CBS public;
	int ret = -1;

	/*
	 * Use the ephemeral values we saved when generating the
	 * ServerKeyExchange message.
	 */
	if ((ecdh = S3I(s)->tmp.ecdh) == NULL) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	/*
	 * Get client's public key from encoded point in the ClientKeyExchange
	 * message.
	 */
	if (!CBS_get_u8_length_prefixed(cbs, &public))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;

	if ((ecdh_peer = EC_KEY_new()) == NULL)
		goto err;

	if (!ssl_kex_peer_public_ecdhe_ecp(ecdh_peer, S3I(s)->tmp.ecdh_nid,
	    &public))
		goto err;

	/* Derive the shared secret and compute master secret. */
	if (!ssl_kex_derive_ecdhe_ecp(ecdh, ecdh_peer, &key, &key_len))
		goto err;
	s->session->master_key_length = tls1_generate_master_secret(s,
	    s->session->master_key, key, key_len);

	EC_KEY_free(S3I(s)->tmp.ecdh);
	S3I(s)->tmp.ecdh = NULL;
	S3I(s)->tmp.ecdh_nid = NID_undef;

	ret = 1;

 err:
	freezero(key, key_len);
	EC_KEY_free(ecdh_peer);

	return (ret);
}

static int
ssl3_get_client_kex_ecdhe_ecx(SSL *s, CBS *cbs)
{
	uint8_t *shared_key = NULL;
	CBS ecpoint;
	int ret = -1;

	if (!CBS_get_u8_length_prefixed(cbs, &ecpoint))
		goto err;
	if (CBS_len(cbs) != 0)
		goto err;
	if (CBS_len(&ecpoint) != X25519_KEY_LENGTH)
		goto err;

	if ((shared_key = malloc(X25519_KEY_LENGTH)) == NULL)
		goto err;
	if (!X25519(shared_key, S3I(s)->tmp.x25519, CBS_data(&ecpoint)))
		goto err;

	freezero(S3I(s)->tmp.x25519, X25519_KEY_LENGTH);
	S3I(s)->tmp.x25519 = NULL;

	s->session->master_key_length =
	    tls1_generate_master_secret(
		s, s->session->master_key, shared_key, X25519_KEY_LENGTH);

	ret = 1;

 err:
	freezero(shared_key, X25519_KEY_LENGTH);

	return (ret);
}

static int
ssl3_get_client_kex_ecdhe(SSL *s, CBS *cbs)
{
	if (S3I(s)->tmp.x25519 != NULL)
		return ssl3_get_client_kex_ecdhe_ecx(s, cbs);

	return ssl3_get_client_kex_ecdhe_ecp(s, cbs);
}

static int
ssl3_get_client_kex_gost(SSL *s, CBS *cbs)
{
	EVP_PKEY_CTX *pkey_ctx;
	EVP_PKEY *client_pub_pkey = NULL, *pk = NULL;
	unsigned char premaster_secret[32];
	unsigned long alg_a;
	size_t outlen = 32;
	CBS gostblob;
	int al;
	int ret = 0;

	/* Get our certificate private key*/
	alg_a = S3I(s)->hs.cipher->algorithm_auth;
	if (alg_a & SSL_aGOST01)
		pk = s->cert->pkeys[SSL_PKEY_GOST01].privatekey;

	if ((pkey_ctx = EVP_PKEY_CTX_new(pk, NULL)) == NULL)
		goto err;
	if (EVP_PKEY_decrypt_init(pkey_ctx) <= 0)
		goto gerr;

	/*
	 * If client certificate is present and is of the same type,
	 * maybe use it for key exchange.
	 * Don't mind errors from EVP_PKEY_derive_set_peer, because
	 * it is completely valid to use a client certificate for
	 * authorization only.
	 */
	if ((client_pub_pkey = X509_get_pubkey(s->session->peer)) != NULL) {
		if (EVP_PKEY_derive_set_peer(pkey_ctx,
		    client_pub_pkey) <= 0)
			ERR_clear_error();
	}

	/* Decrypt session key */
	if (!CBS_get_asn1(cbs, &gostblob, CBS_ASN1_SEQUENCE))
		goto decode_err;
	if (CBS_len(cbs) != 0)
		goto decode_err;
	if (EVP_PKEY_decrypt(pkey_ctx, premaster_secret, &outlen,
	    CBS_data(&gostblob), CBS_len(&gostblob)) <= 0) {
		SSLerror(s, SSL_R_DECRYPTION_FAILED);
		goto gerr;
	}

	/* Generate master secret */
	s->session->master_key_length =
	    tls1_generate_master_secret(
		s, s->session->master_key, premaster_secret, 32);

	/* Check if pubkey from client certificate was used */
	if (EVP_PKEY_CTX_ctrl(pkey_ctx, -1, -1,
	    EVP_PKEY_CTRL_PEER_KEY, 2, NULL) > 0)
		ret = 2;
	else
		ret = 1;
 gerr:
	EVP_PKEY_free(client_pub_pkey);
	EVP_PKEY_CTX_free(pkey_ctx);
	if (ret)
		return (ret);
	else
		goto err;

 decode_err:
	al = SSL_AD_DECODE_ERROR;
	SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (-1);
}

int
ssl3_get_client_key_exchange(SSL *s)
{
	unsigned long alg_k;
	int al, ok;
	CBS cbs;
	long n;

	/* 2048 maxlen is a guess.  How long a key does that permit? */
	n = ssl3_get_message(s, SSL3_ST_SR_KEY_EXCH_A,
	    SSL3_ST_SR_KEY_EXCH_B, SSL3_MT_CLIENT_KEY_EXCHANGE, 2048, &ok);
	if (!ok)
		return ((int)n);

	if (n < 0)
		goto err;

	CBS_init(&cbs, s->internal->init_msg, n);

	alg_k = S3I(s)->hs.cipher->algorithm_mkey;

	if (alg_k & SSL_kRSA) {
		if (ssl3_get_client_kex_rsa(s, &cbs) != 1)
			goto err;
	} else if (alg_k & SSL_kDHE) {
		if (ssl3_get_client_kex_dhe(s, &cbs) != 1)
			goto err;
	} else if (alg_k & SSL_kECDHE) {
		if (ssl3_get_client_kex_ecdhe(s, &cbs) != 1)
			goto err;
	} else if (alg_k & SSL_kGOST) {
		if (ssl3_get_client_kex_gost(s, &cbs) != 1)
			goto err;
	} else {
		al = SSL_AD_HANDSHAKE_FAILURE;
		SSLerror(s, SSL_R_UNKNOWN_CIPHER_TYPE);
		goto fatal_err;
	}

	if (CBS_len(&cbs) != 0) {
		al = SSL_AD_DECODE_ERROR;
		SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
		goto fatal_err;
	}

	return (1);

 fatal_err:
	ssl3_send_alert(s, SSL3_AL_FATAL, al);
 err:
	return (-1);
}

int
ssl3_get_cert_verify(SSL *s)
{
	CBS cbs, signature;
	const struct ssl_sigalg *sigalg = NULL;
	const EVP_MD *md = NULL;
	EVP_PKEY *pkey = NULL;
	X509 *peer = NULL;
	EVP_MD_CTX mctx;
	int al, ok, verify;
	const unsigned char *hdata;
	size_t hdatalen;
	int type = 0;
	int ret = 0;
	long n;

	EVP_MD_CTX_init(&mctx);

	n = ssl3_get_message(s, SSL3_ST_SR_CERT_VRFY_A,
	    SSL3_ST_SR_CERT_VRFY_B, -1, SSL3_RT_MAX_PLAIN_LENGTH, &ok);
	if (!ok)
		return ((int)n);

	if (n < 0)
		goto err;

	CBS_init(&cbs, s->internal->init_msg, n);

	if (s->session->peer != NULL) {
		peer = s->session->peer;
		pkey = X509_get_pubkey(peer);
		type = X509_certificate_type(peer, pkey);
	}

	if (S3I(s)->tmp.message_type != SSL3_MT_CERTIFICATE_VERIFY) {
		S3I(s)->tmp.reuse_message = 1;
		if (peer != NULL) {
			al = SSL_AD_UNEXPECTED_MESSAGE;
			SSLerror(s, SSL_R_MISSING_VERIFY_MESSAGE);
			goto fatal_err;
		}
		ret = 1;
		goto end;
	}

	if (peer == NULL) {
		SSLerror(s, SSL_R_NO_CLIENT_CERT_RECEIVED);
		al = SSL_AD_UNEXPECTED_MESSAGE;
		goto fatal_err;
	}

	if (!(type & EVP_PKT_SIGN)) {
		SSLerror(s, SSL_R_SIGNATURE_FOR_NON_SIGNING_CERTIFICATE);
		al = SSL_AD_ILLEGAL_PARAMETER;
		goto fatal_err;
	}

	if (S3I(s)->change_cipher_spec) {
		SSLerror(s, SSL_R_CCS_RECEIVED_EARLY);
		al = SSL_AD_UNEXPECTED_MESSAGE;
		goto fatal_err;
	}

	if (!SSL_USE_SIGALGS(s)) {
		if (!CBS_get_u16_length_prefixed(&cbs, &signature))
			goto err;
		if (CBS_len(&signature) > EVP_PKEY_size(pkey)) {
			SSLerror(s, SSL_R_WRONG_SIGNATURE_SIZE);
			al = SSL_AD_DECODE_ERROR;
			goto fatal_err;
		}
		if (CBS_len(&cbs) != 0) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_EXTRA_DATA_IN_MESSAGE);
			goto fatal_err;
		}
	}

	if (SSL_USE_SIGALGS(s)) {
		EVP_PKEY_CTX *pctx;
		uint16_t sigalg_value;

		if (!CBS_get_u16(&cbs, &sigalg_value))
			goto decode_err;
		if ((sigalg = ssl_sigalg(sigalg_value, tls12_sigalgs,
		    tls12_sigalgs_len)) == NULL ||
		    (md = sigalg->md()) == NULL) {
			SSLerror(s, SSL_R_UNKNOWN_DIGEST);
			al = SSL_AD_DECODE_ERROR;
			goto fatal_err;
		}
		if (!ssl_sigalg_pkey_ok(sigalg, pkey, 0)) {
			SSLerror(s, SSL_R_WRONG_SIGNATURE_TYPE);
			al = SSL_AD_DECODE_ERROR;
			goto fatal_err;
		}

		if (!CBS_get_u16_length_prefixed(&cbs, &signature))
			goto err;
		if (CBS_len(&signature) > EVP_PKEY_size(pkey)) {
			SSLerror(s, SSL_R_WRONG_SIGNATURE_SIZE);
			al = SSL_AD_DECODE_ERROR;
			goto fatal_err;
		}
		if (CBS_len(&cbs) != 0) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_EXTRA_DATA_IN_MESSAGE);
			goto fatal_err;
		}

		if (!tls1_transcript_data(s, &hdata, &hdatalen)) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (!EVP_DigestVerifyInit(&mctx, &pctx, md, NULL, pkey)) {
			SSLerror(s, ERR_R_EVP_LIB);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if ((sigalg->flags & SIGALG_FLAG_RSA_PSS) &&
		    (!EVP_PKEY_CTX_set_rsa_padding
		    (pctx, RSA_PKCS1_PSS_PADDING) ||
		    !EVP_PKEY_CTX_set_rsa_pss_saltlen(pctx, -1))) {
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (sigalg->key_type == EVP_PKEY_GOSTR01 &&
		    EVP_PKEY_CTX_ctrl(pctx, -1, EVP_PKEY_OP_VERIFY,
		    EVP_PKEY_CTRL_GOST_SIG_FORMAT, GOST_SIG_FORMAT_RS_LE,
		    NULL) <= 0) {
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (!EVP_DigestVerifyUpdate(&mctx, hdata, hdatalen)) {
			SSLerror(s, ERR_R_EVP_LIB);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (EVP_DigestVerifyFinal(&mctx, CBS_data(&signature),
		    CBS_len(&signature)) <= 0) {
			al = SSL_AD_DECRYPT_ERROR;
			SSLerror(s, SSL_R_BAD_SIGNATURE);
			goto fatal_err;
		}
	} else if (pkey->type == EVP_PKEY_RSA) {
		verify = RSA_verify(NID_md5_sha1, S3I(s)->tmp.cert_verify_md,
		    MD5_DIGEST_LENGTH + SHA_DIGEST_LENGTH, CBS_data(&signature),
		    CBS_len(&signature), pkey->pkey.rsa);
		if (verify < 0) {
			al = SSL_AD_DECRYPT_ERROR;
			SSLerror(s, SSL_R_BAD_RSA_DECRYPT);
			goto fatal_err;
		}
		if (verify == 0) {
			al = SSL_AD_DECRYPT_ERROR;
			SSLerror(s, SSL_R_BAD_RSA_SIGNATURE);
			goto fatal_err;
		}
	} else if (pkey->type == EVP_PKEY_EC) {
		verify = ECDSA_verify(pkey->save_type,
		    &(S3I(s)->tmp.cert_verify_md[MD5_DIGEST_LENGTH]),
		    SHA_DIGEST_LENGTH, CBS_data(&signature),
		    CBS_len(&signature), pkey->pkey.ec);
		if (verify <= 0) {
			al = SSL_AD_DECRYPT_ERROR;
			SSLerror(s, SSL_R_BAD_ECDSA_SIGNATURE);
			goto fatal_err;
		}
#ifndef OPENSSL_NO_GOST
	} else if (pkey->type == NID_id_GostR3410_94 ||
	    pkey->type == NID_id_GostR3410_2001) {
		unsigned char sigbuf[128];
		unsigned int siglen = sizeof(sigbuf);
		EVP_PKEY_CTX *pctx;
		int nid;

		if (!tls1_transcript_data(s, &hdata, &hdatalen)) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (!EVP_PKEY_get_default_digest_nid(pkey, &nid) ||
		    !(md = EVP_get_digestbynid(nid))) {
			SSLerror(s, ERR_R_EVP_LIB);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if ((pctx = EVP_PKEY_CTX_new(pkey, NULL)) == NULL) {
			SSLerror(s, ERR_R_EVP_LIB);
			al = SSL_AD_INTERNAL_ERROR;
			goto fatal_err;
		}
		if (!EVP_DigestInit_ex(&mctx, md, NULL) ||
		    !EVP_DigestUpdate(&mctx, hdata, hdatalen) ||
		    !EVP_DigestFinal(&mctx, sigbuf, &siglen) ||
		    (EVP_PKEY_verify_init(pctx) <= 0) ||
		    (EVP_PKEY_CTX_set_signature_md(pctx, md) <= 0) ||
		    (EVP_PKEY_CTX_ctrl(pctx, -1, EVP_PKEY_OP_VERIFY,
		    EVP_PKEY_CTRL_GOST_SIG_FORMAT,
		    GOST_SIG_FORMAT_RS_LE, NULL) <= 0)) {
			SSLerror(s, ERR_R_EVP_LIB);
			al = SSL_AD_INTERNAL_ERROR;
			EVP_PKEY_CTX_free(pctx);
			goto fatal_err;
		}
		if (EVP_PKEY_verify(pctx, CBS_data(&signature),
		    CBS_len(&signature), sigbuf, siglen) <= 0) {
			al = SSL_AD_DECRYPT_ERROR;
			SSLerror(s, SSL_R_BAD_SIGNATURE);
			EVP_PKEY_CTX_free(pctx);
			goto fatal_err;
		}

		EVP_PKEY_CTX_free(pctx);
#endif
	} else {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		al = SSL_AD_UNSUPPORTED_CERTIFICATE;
		goto fatal_err;
	}

	ret = 1;
	if (0) {
 decode_err:
		al = SSL_AD_DECODE_ERROR;
		SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
 fatal_err:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
	}
 end:
	tls1_transcript_free(s);
 err:
	EVP_MD_CTX_cleanup(&mctx);
	EVP_PKEY_free(pkey);
	return (ret);
}

int
ssl3_get_client_certificate(SSL *s)
{
	CBS cbs, client_certs;
	int i, ok, al, ret = -1;
	X509 *x = NULL;
	long n;
	const unsigned char *q;
	STACK_OF(X509) *sk = NULL;

	n = ssl3_get_message(s, SSL3_ST_SR_CERT_A, SSL3_ST_SR_CERT_B,
	    -1, s->internal->max_cert_list, &ok);
	if (!ok)
		return ((int)n);

	if (S3I(s)->tmp.message_type == SSL3_MT_CLIENT_KEY_EXCHANGE) {
		if ((s->verify_mode & SSL_VERIFY_PEER) &&
		    (s->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT)) {
			SSLerror(s, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
			al = SSL_AD_HANDSHAKE_FAILURE;
			goto fatal_err;
		}
		/*
		 * If tls asked for a client cert,
		 * the client must return a 0 list.
		 */
		if (S3I(s)->tmp.cert_request) {
			SSLerror(s, SSL_R_TLS_PEER_DID_NOT_RESPOND_WITH_CERTIFICATE_LIST
			    );
			al = SSL_AD_UNEXPECTED_MESSAGE;
			goto fatal_err;
		}
		S3I(s)->tmp.reuse_message = 1;
		return (1);
	}

	if (S3I(s)->tmp.message_type != SSL3_MT_CERTIFICATE) {
		al = SSL_AD_UNEXPECTED_MESSAGE;
		SSLerror(s, SSL_R_WRONG_MESSAGE_TYPE);
		goto fatal_err;
	}

	if (n < 0)
		goto decode_err;

	CBS_init(&cbs, s->internal->init_msg, n);

	if ((sk = sk_X509_new_null()) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}

	if (!CBS_get_u24_length_prefixed(&cbs, &client_certs) ||
	    CBS_len(&cbs) != 0)
		goto decode_err;

	while (CBS_len(&client_certs) > 0) {
		CBS cert;

		if (!CBS_get_u24_length_prefixed(&client_certs, &cert)) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_CERT_LENGTH_MISMATCH);
			goto fatal_err;
		}

		q = CBS_data(&cert);
		x = d2i_X509(NULL, &q, CBS_len(&cert));
		if (x == NULL) {
			SSLerror(s, ERR_R_ASN1_LIB);
			goto err;
		}
		if (q != CBS_data(&cert) + CBS_len(&cert)) {
			al = SSL_AD_DECODE_ERROR;
			SSLerror(s, SSL_R_CERT_LENGTH_MISMATCH);
			goto fatal_err;
		}
		if (!sk_X509_push(sk, x)) {
			SSLerror(s, ERR_R_MALLOC_FAILURE);
			goto err;
		}
		x = NULL;
	}

	if (sk_X509_num(sk) <= 0) {
		/*
		 * TLS does not mind 0 certs returned.
		 * Fail for TLS only if we required a certificate.
		 */
		if ((s->verify_mode & SSL_VERIFY_PEER) &&
		    (s->verify_mode & SSL_VERIFY_FAIL_IF_NO_PEER_CERT)) {
			SSLerror(s, SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE);
			al = SSL_AD_HANDSHAKE_FAILURE;
			goto fatal_err;
		}
		/* No client certificate so free transcript. */
		tls1_transcript_free(s);
	} else {
		i = ssl_verify_cert_chain(s, sk);
		if (i <= 0) {
			al = ssl_verify_alarm_type(s->verify_result);
			SSLerror(s, SSL_R_NO_CERTIFICATE_RETURNED);
			goto fatal_err;
		}
	}

	X509_free(s->session->peer);
	s->session->peer = sk_X509_shift(sk);
	s->session->verify_result = s->verify_result;

	/*
	 * With the current implementation, sess_cert will always be NULL
	 * when we arrive here
	 */
	if (SSI(s)->sess_cert == NULL) {
		SSI(s)->sess_cert = ssl_sess_cert_new();
		if (SSI(s)->sess_cert == NULL) {
			SSLerror(s, ERR_R_MALLOC_FAILURE);
			goto err;
		}
	}
	sk_X509_pop_free(SSI(s)->sess_cert->cert_chain, X509_free);
	SSI(s)->sess_cert->cert_chain = sk;

	/*
	 * Inconsistency alert: cert_chain does *not* include the
	 * peer's own certificate, while we do include it in s3_clnt.c
	 */

	sk = NULL;

	ret = 1;
	if (0) {
 decode_err:
		al = SSL_AD_DECODE_ERROR;
		SSLerror(s, SSL_R_BAD_PACKET_LENGTH);
 fatal_err:
		ssl3_send_alert(s, SSL3_AL_FATAL, al);
	}
 err:
	X509_free(x);
	sk_X509_pop_free(sk, X509_free);

	return (ret);
}

int
ssl3_send_server_certificate(SSL *s)
{
	CBB cbb, server_cert;
	CERT_PKEY *cpk;

	/*
	 * Server Certificate - RFC 5246, section 7.4.2.
	 */

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_CERT_A) {
		if ((cpk = ssl_get_server_send_pkey(s)) == NULL) {
			SSLerror(s, ERR_R_INTERNAL_ERROR);
			return (0);
		}

		if (!ssl3_handshake_msg_start(s, &cbb, &server_cert,
		    SSL3_MT_CERTIFICATE))
			goto err;
		if (!ssl3_output_cert_chain(s, &server_cert, cpk))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_CERT_B;
	}

	/* SSL3_ST_SW_CERT_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (0);
}

/* send a new session ticket (not necessarily for a new session) */
int
ssl3_send_newsession_ticket(SSL *s)
{
	CBB cbb, session_ticket, ticket;
	SSL_CTX *tctx = s->initial_ctx;
	size_t enc_session_len, enc_session_max_len, hmac_len;
	size_t session_len = 0;
	unsigned char *enc_session = NULL, *session = NULL;
	unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char key_name[16];
	unsigned char *hmac;
	unsigned int hlen;
	EVP_CIPHER_CTX ctx;
	HMAC_CTX hctx;
	int len;

	/*
	 * New Session Ticket - RFC 5077, section 3.3.
	 */

	EVP_CIPHER_CTX_init(&ctx);
	HMAC_CTX_init(&hctx);

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_SESSION_TICKET_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &session_ticket,
		    SSL3_MT_NEWSESSION_TICKET))
			goto err;

		if (!SSL_SESSION_ticket(s->session, &session, &session_len))
			goto err;
		if (session_len > 0xffff)
			goto err;

		/*
		 * Initialize HMAC and cipher contexts. If callback is present
		 * it does all the work, otherwise use generated values from
		 * parent context.
		 */
		if (tctx->internal->tlsext_ticket_key_cb != NULL) {
			if (tctx->internal->tlsext_ticket_key_cb(s,
			    key_name, iv, &ctx, &hctx, 1) < 0) {
				EVP_CIPHER_CTX_cleanup(&ctx);
				goto err;
			}
		} else {
			arc4random_buf(iv, 16);
			EVP_EncryptInit_ex(&ctx, EVP_aes_128_cbc(), NULL,
			    tctx->internal->tlsext_tick_aes_key, iv);
			HMAC_Init_ex(&hctx, tctx->internal->tlsext_tick_hmac_key,
			    16, EVP_sha256(), NULL);
			memcpy(key_name, tctx->internal->tlsext_tick_key_name, 16);
		}

		/* Encrypt the session state. */
		enc_session_max_len = session_len + EVP_MAX_BLOCK_LENGTH;
		if ((enc_session = calloc(1, enc_session_max_len)) == NULL)
			goto err;
		enc_session_len = 0;
		if (!EVP_EncryptUpdate(&ctx, enc_session, &len, session,
		    session_len))
			goto err;
		enc_session_len += len;
		if (!EVP_EncryptFinal_ex(&ctx, enc_session + enc_session_len,
		    &len))
			goto err;
		enc_session_len += len;

		if (enc_session_len > enc_session_max_len)
			goto err;

		/* Generate the HMAC. */
		if (!HMAC_Update(&hctx, key_name, sizeof(key_name)))
			goto err;
		if (!HMAC_Update(&hctx, iv, EVP_CIPHER_CTX_iv_length(&ctx)))
			goto err;
		if (!HMAC_Update(&hctx, enc_session, enc_session_len))
			goto err;

		if ((hmac_len = HMAC_size(&hctx)) <= 0)
			goto err;

		/*
		 * Ticket lifetime hint (advisory only):
		 * We leave this unspecified for resumed session
		 * (for simplicity), and guess that tickets for new
		 * sessions will live as long as their sessions.
		 */
		if (!CBB_add_u32(&session_ticket,
		    s->internal->hit ? 0 : s->session->timeout))
			goto err;

		if (!CBB_add_u16_length_prefixed(&session_ticket, &ticket))
			goto err;
		if (!CBB_add_bytes(&ticket, key_name, sizeof(key_name)))
			goto err;
		if (!CBB_add_bytes(&ticket, iv, EVP_CIPHER_CTX_iv_length(&ctx)))
			goto err;
		if (!CBB_add_bytes(&ticket, enc_session, enc_session_len))
			goto err;
		if (!CBB_add_space(&ticket, &hmac, hmac_len))
			goto err;

		if (!HMAC_Final(&hctx, hmac, &hlen))
			goto err;
		if (hlen != hmac_len)
			goto err;

		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_SESSION_TICKET_B;
	}

	EVP_CIPHER_CTX_cleanup(&ctx);
	HMAC_CTX_cleanup(&hctx);
	freezero(session, session_len);
	free(enc_session);

	/* SSL3_ST_SW_SESSION_TICKET_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);
	EVP_CIPHER_CTX_cleanup(&ctx);
	HMAC_CTX_cleanup(&hctx);
	freezero(session, session_len);
	free(enc_session);

	return (-1);
}

int
ssl3_send_cert_status(SSL *s)
{
	CBB cbb, certstatus, ocspresp;

	memset(&cbb, 0, sizeof(cbb));

	if (S3I(s)->hs.state == SSL3_ST_SW_CERT_STATUS_A) {
		if (!ssl3_handshake_msg_start(s, &cbb, &certstatus,
		    SSL3_MT_CERTIFICATE_STATUS))
			goto err;
		if (!CBB_add_u8(&certstatus, s->tlsext_status_type))
			goto err;
		if (!CBB_add_u24_length_prefixed(&certstatus, &ocspresp))
			goto err;
		if (!CBB_add_bytes(&ocspresp, s->internal->tlsext_ocsp_resp,
		    s->internal->tlsext_ocsp_resp_len))
			goto err;
		if (!ssl3_handshake_msg_finish(s, &cbb))
			goto err;

		S3I(s)->hs.state = SSL3_ST_SW_CERT_STATUS_B;
	}

	/* SSL3_ST_SW_CERT_STATUS_B */
	return (ssl3_handshake_write(s));

 err:
	CBB_cleanup(&cbb);

	return (-1);
}

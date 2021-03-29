/* $OpenBSD: t1_enc.c,v 1.136 2021/03/29 16:19:15 jsing Exp $ */
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

#include <limits.h>
#include <stdio.h>

#include "ssl_locl.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>

int tls1_PRF(SSL *s, const unsigned char *secret, size_t secret_len,
    const void *seed1, size_t seed1_len, const void *seed2, size_t seed2_len,
    const void *seed3, size_t seed3_len, const void *seed4, size_t seed4_len,
    const void *seed5, size_t seed5_len, unsigned char *out, size_t out_len);

void
tls1_cleanup_key_block(SSL *s)
{
	freezero(S3I(s)->hs.tls12.key_block, S3I(s)->hs.tls12.key_block_len);
	S3I(s)->hs.tls12.key_block = NULL;
	S3I(s)->hs.tls12.key_block_len = 0;
}

/*
 * TLS P_hash() data expansion function - see RFC 5246, section 5.
 */
static int
tls1_P_hash(const EVP_MD *md, const unsigned char *secret, size_t secret_len,
    const void *seed1, size_t seed1_len, const void *seed2, size_t seed2_len,
    const void *seed3, size_t seed3_len, const void *seed4, size_t seed4_len,
    const void *seed5, size_t seed5_len, unsigned char *out, size_t out_len)
{
	unsigned char A1[EVP_MAX_MD_SIZE], hmac[EVP_MAX_MD_SIZE];
	size_t A1_len, hmac_len;
	EVP_MD_CTX ctx;
	EVP_PKEY *mac_key;
	int ret = 0;
	int chunk;
	size_t i;

	chunk = EVP_MD_size(md);
	OPENSSL_assert(chunk >= 0);

	EVP_MD_CTX_init(&ctx);

	mac_key = EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, secret, secret_len);
	if (!mac_key)
		goto err;
	if (!EVP_DigestSignInit(&ctx, NULL, md, NULL, mac_key))
		goto err;
	if (seed1 && !EVP_DigestSignUpdate(&ctx, seed1, seed1_len))
		goto err;
	if (seed2 && !EVP_DigestSignUpdate(&ctx, seed2, seed2_len))
		goto err;
	if (seed3 && !EVP_DigestSignUpdate(&ctx, seed3, seed3_len))
		goto err;
	if (seed4 && !EVP_DigestSignUpdate(&ctx, seed4, seed4_len))
		goto err;
	if (seed5 && !EVP_DigestSignUpdate(&ctx, seed5, seed5_len))
		goto err;
	if (!EVP_DigestSignFinal(&ctx, A1, &A1_len))
		goto err;

	for (;;) {
		if (!EVP_DigestSignInit(&ctx, NULL, md, NULL, mac_key))
			goto err;
		if (!EVP_DigestSignUpdate(&ctx, A1, A1_len))
			goto err;
		if (seed1 && !EVP_DigestSignUpdate(&ctx, seed1, seed1_len))
			goto err;
		if (seed2 && !EVP_DigestSignUpdate(&ctx, seed2, seed2_len))
			goto err;
		if (seed3 && !EVP_DigestSignUpdate(&ctx, seed3, seed3_len))
			goto err;
		if (seed4 && !EVP_DigestSignUpdate(&ctx, seed4, seed4_len))
			goto err;
		if (seed5 && !EVP_DigestSignUpdate(&ctx, seed5, seed5_len))
			goto err;
		if (!EVP_DigestSignFinal(&ctx, hmac, &hmac_len))
			goto err;

		if (hmac_len > out_len)
			hmac_len = out_len;

		for (i = 0; i < hmac_len; i++)
			out[i] ^= hmac[i];

		out += hmac_len;
		out_len -= hmac_len;

		if (out_len == 0)
			break;

		if (!EVP_DigestSignInit(&ctx, NULL, md, NULL, mac_key))
			goto err;
		if (!EVP_DigestSignUpdate(&ctx, A1, A1_len))
			goto err;
		if (!EVP_DigestSignFinal(&ctx, A1, &A1_len))
			goto err;
	}
	ret = 1;

 err:
	EVP_PKEY_free(mac_key);
	EVP_MD_CTX_cleanup(&ctx);

	explicit_bzero(A1, sizeof(A1));
	explicit_bzero(hmac, sizeof(hmac));

	return ret;
}

int
tls1_PRF(SSL *s, const unsigned char *secret, size_t secret_len,
    const void *seed1, size_t seed1_len, const void *seed2, size_t seed2_len,
    const void *seed3, size_t seed3_len, const void *seed4, size_t seed4_len,
    const void *seed5, size_t seed5_len, unsigned char *out, size_t out_len)
{
	const EVP_MD *md;
	size_t half_len;

	memset(out, 0, out_len);

	if (!ssl_get_handshake_evp_md(s, &md))
		return (0);

	if (md->type == NID_md5_sha1) {
		/*
		 * Partition secret between MD5 and SHA1, then XOR result.
		 * If the secret length is odd, a one byte overlap is used.
		 */
		half_len = secret_len - (secret_len / 2);
		if (!tls1_P_hash(EVP_md5(), secret, half_len, seed1, seed1_len,
		    seed2, seed2_len, seed3, seed3_len, seed4, seed4_len,
		    seed5, seed5_len, out, out_len))
			return (0);

		secret += secret_len - half_len;
		if (!tls1_P_hash(EVP_sha1(), secret, half_len, seed1, seed1_len,
		    seed2, seed2_len, seed3, seed3_len, seed4, seed4_len,
		    seed5, seed5_len, out, out_len))
			return (0);

		return (1);
	}

	if (!tls1_P_hash(md, secret, secret_len, seed1, seed1_len,
	    seed2, seed2_len, seed3, seed3_len, seed4, seed4_len,
	    seed5, seed5_len, out, out_len))
		return (0);

	return (1);
}

static int
tls1_generate_key_block(SSL *s, uint8_t *key_block, size_t key_block_len)
{
	return tls1_PRF(s,
	    s->session->master_key, s->session->master_key_length,
	    TLS_MD_KEY_EXPANSION_CONST, TLS_MD_KEY_EXPANSION_CONST_SIZE,
	    s->s3->server_random, SSL3_RANDOM_SIZE,
	    s->s3->client_random, SSL3_RANDOM_SIZE,
	    NULL, 0, NULL, 0, key_block, key_block_len);
}

int
tls1_change_cipher_state(SSL *s, int which)
{
	const unsigned char *client_write_mac_secret, *server_write_mac_secret;
	const unsigned char *client_write_key, *server_write_key;
	const unsigned char *client_write_iv, *server_write_iv;
	const unsigned char *mac_secret, *key, *iv;
	int mac_secret_size, key_len, iv_len;
	unsigned char *key_block;
	const EVP_CIPHER *cipher;
	const EVP_AEAD *aead;
	char is_read, use_client_keys;

	cipher = S3I(s)->tmp.new_sym_enc;
	aead = S3I(s)->tmp.new_aead;

	/*
	 * is_read is true if we have just read a ChangeCipherSpec message,
	 * that is we need to update the read cipherspec. Otherwise we have
	 * just written one.
	 */
	is_read = (which & SSL3_CC_READ) != 0;

	/*
	 * use_client_keys is true if we wish to use the keys for the "client
	 * write" direction. This is the case if we're a client sending a
	 * ChangeCipherSpec, or a server reading a client's ChangeCipherSpec.
	 */
	use_client_keys = ((which == SSL3_CHANGE_CIPHER_CLIENT_WRITE) ||
	    (which == SSL3_CHANGE_CIPHER_SERVER_READ));

	if (aead != NULL) {
		key_len = EVP_AEAD_key_length(aead);
		iv_len = SSL_CIPHER_AEAD_FIXED_NONCE_LEN(S3I(s)->hs.cipher);
	} else {
		key_len = EVP_CIPHER_key_length(cipher);
		iv_len = EVP_CIPHER_iv_length(cipher);
	}

	mac_secret_size = S3I(s)->tmp.new_mac_secret_size;

	key_block = S3I(s)->hs.tls12.key_block;
	client_write_mac_secret = key_block;
	key_block += mac_secret_size;
	server_write_mac_secret = key_block;
	key_block += mac_secret_size;
	client_write_key = key_block;
	key_block += key_len;
	server_write_key = key_block;
	key_block += key_len;
	client_write_iv = key_block;
	key_block += iv_len;
	server_write_iv = key_block;
	key_block += iv_len;

	if (use_client_keys) {
		mac_secret = client_write_mac_secret;
		key = client_write_key;
		iv = client_write_iv;
	} else {
		mac_secret = server_write_mac_secret;
		key = server_write_key;
		iv = server_write_iv;
	}

	if (key_block - S3I(s)->hs.tls12.key_block !=
	    S3I(s)->hs.tls12.key_block_len) {
		SSLerror(s, ERR_R_INTERNAL_ERROR);
		goto err;
	}

	if (is_read) {
		if (!tls12_record_layer_change_read_cipher_state(s->internal->rl,
		    mac_secret, mac_secret_size, key, key_len, iv, iv_len))
			goto err;
		tls12_record_layer_read_cipher_hash(s->internal->rl,
		    &s->enc_read_ctx, &s->read_hash);
	} else {
		if (!tls12_record_layer_change_write_cipher_state(s->internal->rl,
		    mac_secret, mac_secret_size, key, key_len, iv, iv_len))
			goto err;
	}
	return (1);

 err:
	return (0);
}

int
tls1_setup_key_block(SSL *s)
{
	unsigned char *key_block;
	int mac_type = NID_undef, mac_secret_size = 0;
	size_t key_block_len;
	int key_len, iv_len;
	const EVP_CIPHER *cipher = NULL;
	const EVP_AEAD *aead = NULL;
	const EVP_MD *handshake_hash = NULL;
	const EVP_MD *mac_hash = NULL;
	int ret = 0;

	if (S3I(s)->hs.tls12.key_block_len != 0)
		return (1);

	if (s->session->cipher &&
	    (s->session->cipher->algorithm_mac & SSL_AEAD)) {
		if (!ssl_cipher_get_evp_aead(s->session, &aead)) {
			SSLerror(s, SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
			return (0);
		}
		key_len = EVP_AEAD_key_length(aead);
		iv_len = SSL_CIPHER_AEAD_FIXED_NONCE_LEN(s->session->cipher);
	} else {
		if (!ssl_cipher_get_evp(s->session, &cipher, &mac_hash,
		    &mac_type, &mac_secret_size)) {
			SSLerror(s, SSL_R_CIPHER_OR_HASH_UNAVAILABLE);
			return (0);
		}
		key_len = EVP_CIPHER_key_length(cipher);
		iv_len = EVP_CIPHER_iv_length(cipher);
	}

	if (!ssl_get_handshake_evp_md(s, &handshake_hash))
		return (0);

	S3I(s)->tmp.new_aead = aead;
	S3I(s)->tmp.new_sym_enc = cipher;
	S3I(s)->tmp.new_mac_secret_size = mac_secret_size;

	tls12_record_layer_set_aead(s->internal->rl, aead);
	tls12_record_layer_set_cipher_hash(s->internal->rl, cipher,
	    handshake_hash, mac_hash);

	tls1_cleanup_key_block(s);

	if ((key_block = reallocarray(NULL, mac_secret_size + key_len + iv_len,
	    2)) == NULL) {
		SSLerror(s, ERR_R_MALLOC_FAILURE);
		goto err;
	}
	key_block_len = (mac_secret_size + key_len + iv_len) * 2;

	S3I(s)->hs.tls12.key_block_len = key_block_len;
	S3I(s)->hs.tls12.key_block = key_block;

	if (!tls1_generate_key_block(s, key_block, key_block_len))
		goto err;

	if (!(s->internal->options & SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) &&
	    s->method->internal->version <= TLS1_VERSION) {
		/*
		 * Enable vulnerability countermeasure for CBC ciphers with
		 * known-IV problem (http://www.openssl.org/~bodo/tls-cbc.txt)
		 */
		S3I(s)->need_empty_fragments = 1;

		if (s->session->cipher != NULL) {
			if (s->session->cipher->algorithm_enc == SSL_eNULL)
				S3I(s)->need_empty_fragments = 0;

#ifndef OPENSSL_NO_RC4
			if (s->session->cipher->algorithm_enc == SSL_RC4)
				S3I(s)->need_empty_fragments = 0;
#endif
		}
	}

	ret = 1;

 err:
	return (ret);
}

int
tls1_final_finish_mac(SSL *s, const char *str, int str_len, unsigned char *out)
{
	unsigned char buf[EVP_MAX_MD_SIZE];
	size_t hash_len;

	if (str_len < 0)
		return 0;

	if (!tls1_transcript_hash_value(s, buf, sizeof(buf), &hash_len))
		return 0;

	if (!tls1_PRF(s, s->session->master_key, s->session->master_key_length,
	    str, str_len, buf, hash_len, NULL, 0, NULL, 0, NULL, 0,
	    out, TLS1_FINISH_MAC_LENGTH))
		return 0;

	return TLS1_FINISH_MAC_LENGTH;
}

int
tls1_generate_master_secret(SSL *s, unsigned char *out, unsigned char *p,
    int len)
{
	if (len < 0)
		return 0;

	if (!tls1_PRF(s, p, len,
	    TLS_MD_MASTER_SECRET_CONST, TLS_MD_MASTER_SECRET_CONST_SIZE,
	    s->s3->client_random, SSL3_RANDOM_SIZE, NULL, 0,
	    s->s3->server_random, SSL3_RANDOM_SIZE, NULL, 0,
	    s->session->master_key, SSL_MAX_MASTER_KEY_LENGTH))
		return 0;

	return (SSL_MAX_MASTER_KEY_LENGTH);
}

int
tls1_export_keying_material(SSL *s, unsigned char *out, size_t olen,
    const char *label, size_t llen, const unsigned char *context,
    size_t contextlen, int use_context)
{
	unsigned char *val = NULL;
	size_t vallen, currentvalpos;
	int rv;

	if (!SSL_is_init_finished(s)) {
		SSLerror(s, SSL_R_BAD_STATE);
		return 0;
	}

	/* construct PRF arguments
	 * we construct the PRF argument ourself rather than passing separate
	 * values into the TLS PRF to ensure that the concatenation of values
	 * does not create a prohibited label.
	 */
	vallen = llen + SSL3_RANDOM_SIZE * 2;
	if (use_context) {
		vallen += 2 + contextlen;
	}

	val = malloc(vallen);
	if (val == NULL)
		goto err2;
	currentvalpos = 0;
	memcpy(val + currentvalpos, (unsigned char *) label, llen);
	currentvalpos += llen;
	memcpy(val + currentvalpos, s->s3->client_random, SSL3_RANDOM_SIZE);
	currentvalpos += SSL3_RANDOM_SIZE;
	memcpy(val + currentvalpos, s->s3->server_random, SSL3_RANDOM_SIZE);
	currentvalpos += SSL3_RANDOM_SIZE;

	if (use_context) {
		val[currentvalpos] = (contextlen >> 8) & 0xff;
		currentvalpos++;
		val[currentvalpos] = contextlen & 0xff;
		currentvalpos++;
		if ((contextlen > 0) || (context != NULL)) {
			memcpy(val + currentvalpos, context, contextlen);
		}
	}

	/* disallow prohibited labels
	 * note that SSL3_RANDOM_SIZE > max(prohibited label len) =
	 * 15, so size of val > max(prohibited label len) = 15 and the
	 * comparisons won't have buffer overflow
	 */
	if (memcmp(val, TLS_MD_CLIENT_FINISH_CONST,
	    TLS_MD_CLIENT_FINISH_CONST_SIZE) == 0)
		goto err1;
	if (memcmp(val, TLS_MD_SERVER_FINISH_CONST,
	    TLS_MD_SERVER_FINISH_CONST_SIZE) == 0)
		goto err1;
	if (memcmp(val, TLS_MD_MASTER_SECRET_CONST,
	    TLS_MD_MASTER_SECRET_CONST_SIZE) == 0)
		goto err1;
	if (memcmp(val, TLS_MD_KEY_EXPANSION_CONST,
	    TLS_MD_KEY_EXPANSION_CONST_SIZE) == 0)
		goto err1;

	rv = tls1_PRF(s, s->session->master_key, s->session->master_key_length,
	    val, vallen, NULL, 0, NULL, 0, NULL, 0, NULL, 0, out, olen);

	goto ret;
err1:
	SSLerror(s, SSL_R_TLS_ILLEGAL_EXPORTER_LABEL);
	rv = 0;
	goto ret;
err2:
	SSLerror(s, ERR_R_MALLOC_FAILURE);
	rv = 0;
ret:
	free(val);

	return (rv);
}

int
tls1_alert_code(int code)
{
	switch (code) {
	case SSL_AD_CLOSE_NOTIFY:
		return (SSL3_AD_CLOSE_NOTIFY);
	case SSL_AD_UNEXPECTED_MESSAGE:
		return (SSL3_AD_UNEXPECTED_MESSAGE);
	case SSL_AD_BAD_RECORD_MAC:
		return (SSL3_AD_BAD_RECORD_MAC);
	case SSL_AD_DECRYPTION_FAILED:
		return (TLS1_AD_DECRYPTION_FAILED);
	case SSL_AD_RECORD_OVERFLOW:
		return (TLS1_AD_RECORD_OVERFLOW);
	case SSL_AD_DECOMPRESSION_FAILURE:
		return (SSL3_AD_DECOMPRESSION_FAILURE);
	case SSL_AD_HANDSHAKE_FAILURE:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_NO_CERTIFICATE:
		return (-1);
	case SSL_AD_BAD_CERTIFICATE:
		return (SSL3_AD_BAD_CERTIFICATE);
	case SSL_AD_UNSUPPORTED_CERTIFICATE:
		return (SSL3_AD_UNSUPPORTED_CERTIFICATE);
	case SSL_AD_CERTIFICATE_REVOKED:
		return (SSL3_AD_CERTIFICATE_REVOKED);
	case SSL_AD_CERTIFICATE_EXPIRED:
		return (SSL3_AD_CERTIFICATE_EXPIRED);
	case SSL_AD_CERTIFICATE_UNKNOWN:
		return (SSL3_AD_CERTIFICATE_UNKNOWN);
	case SSL_AD_ILLEGAL_PARAMETER:
		return (SSL3_AD_ILLEGAL_PARAMETER);
	case SSL_AD_UNKNOWN_CA:
		return (TLS1_AD_UNKNOWN_CA);
	case SSL_AD_ACCESS_DENIED:
		return (TLS1_AD_ACCESS_DENIED);
	case SSL_AD_DECODE_ERROR:
		return (TLS1_AD_DECODE_ERROR);
	case SSL_AD_DECRYPT_ERROR:
		return (TLS1_AD_DECRYPT_ERROR);
	case SSL_AD_EXPORT_RESTRICTION:
		return (TLS1_AD_EXPORT_RESTRICTION);
	case SSL_AD_PROTOCOL_VERSION:
		return (TLS1_AD_PROTOCOL_VERSION);
	case SSL_AD_INSUFFICIENT_SECURITY:
		return (TLS1_AD_INSUFFICIENT_SECURITY);
	case SSL_AD_INTERNAL_ERROR:
		return (TLS1_AD_INTERNAL_ERROR);
	case SSL_AD_INAPPROPRIATE_FALLBACK:
		return(TLS1_AD_INAPPROPRIATE_FALLBACK);
	case SSL_AD_USER_CANCELLED:
		return (TLS1_AD_USER_CANCELLED);
	case SSL_AD_NO_RENEGOTIATION:
		return (TLS1_AD_NO_RENEGOTIATION);
	case SSL_AD_UNSUPPORTED_EXTENSION:
		return (TLS1_AD_UNSUPPORTED_EXTENSION);
	case SSL_AD_CERTIFICATE_UNOBTAINABLE:
		return (TLS1_AD_CERTIFICATE_UNOBTAINABLE);
	case SSL_AD_UNRECOGNIZED_NAME:
		return (TLS1_AD_UNRECOGNIZED_NAME);
	case SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
		return (TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE);
	case SSL_AD_BAD_CERTIFICATE_HASH_VALUE:
		return (TLS1_AD_BAD_CERTIFICATE_HASH_VALUE);
	case SSL_AD_UNKNOWN_PSK_IDENTITY:
		return (TLS1_AD_UNKNOWN_PSK_IDENTITY);
	default:
		return (-1);
	}
}

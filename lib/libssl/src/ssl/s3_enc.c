/* $OpenBSD: s3_enc.c,v 1.68 2015/09/11 17:04:39 jsing Exp $ */
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

#include <stdio.h>

#include "ssl_locl.h"

#include <openssl/evp.h>
#include <openssl/md5.h>

static unsigned char ssl3_pad_1[48] = {
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36,
	0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36, 0x36
};

static unsigned char ssl3_pad_2[48] = {
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c,
	0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c, 0x5c
};

static int ssl3_handshake_mac(SSL *s, int md_nid, const char *sender,
    int len, unsigned char *p);

void
ssl3_cleanup_key_block(SSL *s)
{
	if (s->s3->tmp.key_block != NULL) {
		explicit_bzero(s->s3->tmp.key_block,
		    s->s3->tmp.key_block_length);
		free(s->s3->tmp.key_block);
		s->s3->tmp.key_block = NULL;
	}
	s->s3->tmp.key_block_length = 0;
}

int
ssl3_init_finished_mac(SSL *s)
{
	BIO_free(s->s3->handshake_buffer);
	ssl3_free_digest_list(s);

	s->s3->handshake_buffer = BIO_new(BIO_s_mem());
	if (s->s3->handshake_buffer == NULL)
		return (0);

	(void)BIO_set_close(s->s3->handshake_buffer, BIO_CLOSE);

	return (1);
}

void
ssl3_free_digest_list(SSL *s)
{
	int i;

	if (s == NULL)
		return;

	if (s->s3->handshake_dgst == NULL)
		return;
	for (i = 0; i < SSL_MAX_DIGEST; i++) {
		if (s->s3->handshake_dgst[i])
			EVP_MD_CTX_destroy(s->s3->handshake_dgst[i]);
	}
	free(s->s3->handshake_dgst);
	s->s3->handshake_dgst = NULL;
}

void
ssl3_finish_mac(SSL *s, const unsigned char *buf, int len)
{
	if (s->s3->handshake_buffer &&
	    !(s->s3->flags & TLS1_FLAGS_KEEP_HANDSHAKE)) {
		BIO_write(s->s3->handshake_buffer, (void *)buf, len);
	} else {
		int i;
		for (i = 0; i < SSL_MAX_DIGEST; i++) {
			if (s->s3->handshake_dgst[i]!= NULL)
				EVP_DigestUpdate(s->s3->handshake_dgst[i], buf, len);
		}
	}
}

int
ssl3_digest_cached_records(SSL *s)
{
	int i;
	long mask;
	const EVP_MD *md;
	long hdatalen;
	void *hdata;

	ssl3_free_digest_list(s);

	s->s3->handshake_dgst = calloc(SSL_MAX_DIGEST, sizeof(EVP_MD_CTX *));
	if (s->s3->handshake_dgst == NULL) {
		SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS, ERR_R_MALLOC_FAILURE);
		return 0;
	}
	hdatalen = BIO_get_mem_data(s->s3->handshake_buffer, &hdata);
	if (hdatalen <= 0) {
		SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS,
		    SSL_R_BAD_HANDSHAKE_LENGTH);
		return 0;
	}

	/* Loop through bits of the algorithm2 field and create MD contexts. */
	for (i = 0; ssl_get_handshake_digest(i, &mask, &md); i++) {
		if ((mask & ssl_get_algorithm2(s)) && md) {
			s->s3->handshake_dgst[i] = EVP_MD_CTX_create();
			if (s->s3->handshake_dgst[i] == NULL) {
				SSLerr(SSL_F_SSL3_DIGEST_CACHED_RECORDS,
				    ERR_R_MALLOC_FAILURE);
				return 0;
			}
			if (!EVP_DigestInit_ex(s->s3->handshake_dgst[i],
			    md, NULL)) {
				EVP_MD_CTX_destroy(s->s3->handshake_dgst[i]);
				return 0;
			}
			if (!EVP_DigestUpdate(s->s3->handshake_dgst[i], hdata,
			    hdatalen))
				return 0;
		}
	}

	if (!(s->s3->flags & TLS1_FLAGS_KEEP_HANDSHAKE)) {
		BIO_free(s->s3->handshake_buffer);
		s->s3->handshake_buffer = NULL;
	}

	return 1;
}

int
ssl3_cert_verify_mac(SSL *s, int md_nid, unsigned char *p)
{
	return (ssl3_handshake_mac(s, md_nid, NULL, 0, p));
}

static int
ssl3_handshake_mac(SSL *s, int md_nid, const char *sender, int len,
    unsigned char *p)
{
	unsigned int ret;
	int npad, n;
	unsigned int i;
	unsigned char md_buf[EVP_MAX_MD_SIZE];
	EVP_MD_CTX ctx, *d = NULL;

	if (s->s3->handshake_buffer)
		if (!ssl3_digest_cached_records(s))
			return 0;

	/* Search for digest of specified type in the handshake_dgst array. */
	for (i = 0; i < SSL_MAX_DIGEST; i++) {
		if (s->s3->handshake_dgst[i] &&
		    EVP_MD_CTX_type(s->s3->handshake_dgst[i]) == md_nid) {
			d = s->s3->handshake_dgst[i];
			break;
		}
	}
	if (!d) {
		SSLerr(SSL_F_SSL3_HANDSHAKE_MAC, SSL_R_NO_REQUIRED_DIGEST);
		return 0;
	}
	EVP_MD_CTX_init(&ctx);
	if (!EVP_MD_CTX_copy_ex(&ctx, d))
		return 0;
	n = EVP_MD_CTX_size(&ctx);
	if (n < 0)
		return 0;

	npad = (48 / n) * n;
	if (sender != NULL)
		EVP_DigestUpdate(&ctx, sender, len);
	EVP_DigestUpdate(&ctx, s->session->master_key,
	    s->session->master_key_length);
	EVP_DigestUpdate(&ctx, ssl3_pad_1, npad);
	EVP_DigestFinal_ex(&ctx, md_buf, &i);

	if (!EVP_DigestInit_ex(&ctx, EVP_MD_CTX_md(&ctx), NULL))
		return 0;
	EVP_DigestUpdate(&ctx, s->session->master_key,
	    s->session->master_key_length);
	EVP_DigestUpdate(&ctx, ssl3_pad_2, npad);
	EVP_DigestUpdate(&ctx, md_buf, i);
	EVP_DigestFinal_ex(&ctx, p, &ret);

	EVP_MD_CTX_cleanup(&ctx);

	return ((int)ret);
}

void
ssl3_record_sequence_increment(unsigned char *seq)
{
	int i;

	for (i = SSL3_SEQUENCE_SIZE - 1; i >= 0; i--) {
		if (++seq[i] != 0)
			break;
	}
}

int
ssl3_alert_code(int code)
{
	switch (code) {
	case SSL_AD_CLOSE_NOTIFY:
		return (SSL3_AD_CLOSE_NOTIFY);
	case SSL_AD_UNEXPECTED_MESSAGE:
		return (SSL3_AD_UNEXPECTED_MESSAGE);
	case SSL_AD_BAD_RECORD_MAC:
		return (SSL3_AD_BAD_RECORD_MAC);
	case SSL_AD_DECRYPTION_FAILED:
		return (SSL3_AD_BAD_RECORD_MAC);
	case SSL_AD_RECORD_OVERFLOW:
		return (SSL3_AD_BAD_RECORD_MAC);
	case SSL_AD_DECOMPRESSION_FAILURE:
		return (SSL3_AD_DECOMPRESSION_FAILURE);
	case SSL_AD_HANDSHAKE_FAILURE:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_NO_CERTIFICATE:
		return (SSL3_AD_NO_CERTIFICATE);
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
		return (SSL3_AD_BAD_CERTIFICATE);
	case SSL_AD_ACCESS_DENIED:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_DECODE_ERROR:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_DECRYPT_ERROR:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_EXPORT_RESTRICTION:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_PROTOCOL_VERSION:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_INSUFFICIENT_SECURITY:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_INTERNAL_ERROR:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_INAPPROPRIATE_FALLBACK:
		return (TLS1_AD_INAPPROPRIATE_FALLBACK);
	case SSL_AD_USER_CANCELLED:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_NO_RENEGOTIATION:
		return(-1); /* Don't send it :-) */
	case SSL_AD_UNSUPPORTED_EXTENSION:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_CERTIFICATE_UNOBTAINABLE:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_UNRECOGNIZED_NAME:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_BAD_CERTIFICATE_HASH_VALUE:
		return (SSL3_AD_HANDSHAKE_FAILURE);
	case SSL_AD_UNKNOWN_PSK_IDENTITY:
		return (TLS1_AD_UNKNOWN_PSK_IDENTITY);
	default:
		return (-1);
	}
}

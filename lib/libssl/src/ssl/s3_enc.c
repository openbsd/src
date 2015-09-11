/* $OpenBSD: s3_enc.c,v 1.70 2015/09/11 17:11:53 jsing Exp $ */
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

void
ssl3_record_sequence_increment(unsigned char *seq)
{
	int i;

	for (i = SSL3_SEQUENCE_SIZE - 1; i >= 0; i--) {
		if (++seq[i] != 0)
			break;
	}
}

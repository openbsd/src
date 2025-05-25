/* $OpenBSD: aes_ige.c,v 1.11 2025/05/25 06:24:37 jsing Exp $ */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
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
 */

#include <openssl/aes.h>
#include <openssl/crypto.h>

#include "aes_local.h"

#define N_WORDS (AES_BLOCK_SIZE / sizeof(unsigned long))
typedef struct {
	unsigned long data[N_WORDS];
} aes_block_t;

void
AES_ige_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const AES_KEY *key, unsigned char *ivec, const int enc)
{
	aes_block_t tmp, tmp2;
	aes_block_t iv;
	aes_block_t iv2;
	size_t n;
	size_t len;

	/* N.B. The IV for this mode is _twice_ the block size */

	OPENSSL_assert((length % AES_BLOCK_SIZE) == 0);

	len = length / AES_BLOCK_SIZE;

	memcpy(iv.data, ivec, AES_BLOCK_SIZE);
	memcpy(iv2.data, ivec + AES_BLOCK_SIZE, AES_BLOCK_SIZE);

	if (AES_ENCRYPT == enc) {
		while (len) {
			memcpy(tmp.data, in, AES_BLOCK_SIZE);
			for (n = 0; n < N_WORDS; ++n)
				tmp2.data[n] = tmp.data[n] ^ iv.data[n];
			AES_encrypt((unsigned char *)tmp2.data,
			    (unsigned char *)tmp2.data, key);
			for (n = 0; n < N_WORDS; ++n)
				tmp2.data[n] ^= iv2.data[n];
			memcpy(out, tmp2.data, AES_BLOCK_SIZE);
			iv = tmp2;
			iv2 = tmp;
			--len;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
	} else {
		while (len) {
			memcpy(tmp.data, in, AES_BLOCK_SIZE);
			tmp2 = tmp;
			for (n = 0; n < N_WORDS; ++n)
				tmp.data[n] ^= iv2.data[n];
			AES_decrypt((unsigned char *)tmp.data,
			    (unsigned char *)tmp.data, key);
			for (n = 0; n < N_WORDS; ++n)
				tmp.data[n] ^= iv.data[n];
			memcpy(out, tmp.data, AES_BLOCK_SIZE);
			iv = tmp2;
			iv2 = tmp;
			--len;
			in += AES_BLOCK_SIZE;
			out += AES_BLOCK_SIZE;
		}
	}
	memcpy(ivec, iv.data, AES_BLOCK_SIZE);
	memcpy(ivec + AES_BLOCK_SIZE, iv2.data, AES_BLOCK_SIZE);
}
LCRYPTO_ALIAS(AES_ige_encrypt);

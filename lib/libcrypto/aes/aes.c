/* $OpenBSD: aes.c,v 1.3 2024/03/30 05:14:12 joshua Exp $ */
/* ====================================================================
 * Copyright (c) 2002-2006 The OpenSSL Project.  All rights reserved.
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

#include <string.h>

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/modes.h>

static const unsigned char aes_wrap_default_iv[] = {
	0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6,
};

#ifdef HAVE_AES_CBC_ENCRYPT_INTERNAL
void aes_cbc_encrypt_internal(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc);

#else
static inline void
aes_cbc_encrypt_internal(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc)
{
	if (enc)
		CRYPTO_cbc128_encrypt(in, out, len, key, ivec,
		    (block128_f)AES_encrypt);
	else
		CRYPTO_cbc128_decrypt(in, out, len, key, ivec,
		    (block128_f)AES_decrypt);
}
#endif

void
AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
    size_t len, const AES_KEY *key, unsigned char *ivec, const int enc)
{
	aes_cbc_encrypt_internal(in, out, len, key, ivec, enc);
}
LCRYPTO_ALIAS(AES_cbc_encrypt);

/*
 * The input and output encrypted as though 128bit cfb mode is being
 * used.  The extra state information to record how much of the
 * 128bit block we have used is contained in *num;
 */

void
AES_cfb128_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const AES_KEY *key, unsigned char *ivec, int *num, const int enc)
{
	CRYPTO_cfb128_encrypt(in, out, length, key, ivec, num, enc,
	    (block128_f)AES_encrypt);
}
LCRYPTO_ALIAS(AES_cfb128_encrypt);

/* N.B. This expects the input to be packed, MS bit first */
void
AES_cfb1_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const AES_KEY *key, unsigned char *ivec, int *num, const int enc)
{
	CRYPTO_cfb128_1_encrypt(in, out, length, key, ivec, num, enc,
	    (block128_f)AES_encrypt);
}
LCRYPTO_ALIAS(AES_cfb1_encrypt);

void
AES_cfb8_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const AES_KEY *key, unsigned char *ivec, int *num, const int enc)
{
	CRYPTO_cfb128_8_encrypt(in, out, length, key, ivec, num, enc,
	    (block128_f)AES_encrypt);
}
LCRYPTO_ALIAS(AES_cfb8_encrypt);

void
AES_ctr128_encrypt(const unsigned char *in, unsigned char *out,
    size_t length, const AES_KEY *key, unsigned char ivec[AES_BLOCK_SIZE],
    unsigned char ecount_buf[AES_BLOCK_SIZE], unsigned int *num)
{
	CRYPTO_ctr128_encrypt(in, out, length, key, ivec, ecount_buf, num,
	    (block128_f)AES_encrypt);
}
LCRYPTO_ALIAS(AES_ctr128_encrypt);

void
AES_ecb_encrypt(const unsigned char *in, unsigned char *out,
    const AES_KEY *key, const int enc)
{
	if (AES_ENCRYPT == enc)
		AES_encrypt(in, out, key);
	else
		AES_decrypt(in, out, key);
}
LCRYPTO_ALIAS(AES_ecb_encrypt);

void
AES_ofb128_encrypt(const unsigned char *in, unsigned char *out, size_t length,
    const AES_KEY *key, unsigned char *ivec, int *num)
{
	CRYPTO_ofb128_encrypt(in, out, length, key, ivec, num,
	    (block128_f)AES_encrypt);
}
LCRYPTO_ALIAS(AES_ofb128_encrypt);

int
AES_wrap_key(AES_KEY *key, const unsigned char *iv, unsigned char *out,
    const unsigned char *in, unsigned int inlen)
{
	unsigned char *A, B[16], *R;
	unsigned int i, j, t;

	if ((inlen & 0x7) || (inlen < 16))
		return -1;
	A = B;
	t = 1;
	memmove(out + 8, in, inlen);
	if (!iv)
		iv = aes_wrap_default_iv;

	memcpy(A, iv, 8);

	for (j = 0; j < 6; j++) {
		R = out + 8;
		for (i = 0; i < inlen; i += 8, t++, R += 8) {
			memcpy(B + 8, R, 8);
			AES_encrypt(B, B, key);
			A[7] ^= (unsigned char)(t & 0xff);
			if (t > 0xff) {
				A[6] ^= (unsigned char)((t >> 8) & 0xff);
				A[5] ^= (unsigned char)((t >> 16) & 0xff);
				A[4] ^= (unsigned char)((t >> 24) & 0xff);
			}
			memcpy(R, B + 8, 8);
		}
	}
	memcpy(out, A, 8);
	return inlen + 8;
}
LCRYPTO_ALIAS(AES_wrap_key);

int
AES_unwrap_key(AES_KEY *key, const unsigned char *iv, unsigned char *out,
    const unsigned char *in, unsigned int inlen)
{
	unsigned char *A, B[16], *R;
	unsigned int i, j, t;

	if ((inlen & 0x7) || (inlen < 24))
		return -1;
	inlen -= 8;
	A = B;
	t = 6 * (inlen >> 3);
	memcpy(A, in, 8);
	memmove(out, in + 8, inlen);
	for (j = 0; j < 6; j++) {
		R = out + inlen - 8;
		for (i = 0; i < inlen; i += 8, t--, R -= 8) {
			A[7] ^= (unsigned char)(t & 0xff);
			if (t > 0xff) {
				A[6] ^= (unsigned char)((t >> 8) & 0xff);
				A[5] ^= (unsigned char)((t >> 16) & 0xff);
				A[4] ^= (unsigned char)((t >> 24) & 0xff);
			}
			memcpy(B + 8, R, 8);
			AES_decrypt(B, B, key);
			memcpy(R, B + 8, 8);
		}
	}
	if (!iv)
		iv = aes_wrap_default_iv;
	if (memcmp(A, iv, 8)) {
		explicit_bzero(out, inlen);
		return 0;
	}
	return inlen;
}
LCRYPTO_ALIAS(AES_unwrap_key);

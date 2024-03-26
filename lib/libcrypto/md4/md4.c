/* $OpenBSD: md4.c,v 1.9 2024/03/26 06:40:29 jsing Exp $ */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>
#include <openssl/md4.h>

__BEGIN_HIDDEN_DECLS

void md4_block_data_order (MD4_CTX *c, const void *p, size_t num);

__END_HIDDEN_DECLS

#define DATA_ORDER_IS_LITTLE_ENDIAN

#define HASH_LONG		MD4_LONG
#define HASH_CTX		MD4_CTX
#define HASH_CBLOCK		MD4_CBLOCK
#define HASH_UPDATE		MD4_Update
#define HASH_TRANSFORM		MD4_Transform
#define HASH_FINAL		MD4_Final
#define	HASH_MAKE_STRING(c,s)	do {	\
	unsigned long ll;		\
	ll=(c)->A; HOST_l2c(ll,(s));	\
	ll=(c)->B; HOST_l2c(ll,(s));	\
	ll=(c)->C; HOST_l2c(ll,(s));	\
	ll=(c)->D; HOST_l2c(ll,(s));	\
	} while (0)
#define	HASH_BLOCK_DATA_ORDER	md4_block_data_order

#define HASH_NO_UPDATE
#define HASH_NO_TRANSFORM
#define HASH_NO_FINAL

#include "md32_common.h"

int
HASH_UPDATE(HASH_CTX *c, const void *data_, size_t len)
{
	const unsigned char *data = data_;
	unsigned char *p;
	HASH_LONG l;
	size_t n;

	if (len == 0)
		return 1;

	l = (c->Nl + (((HASH_LONG)len) << 3))&0xffffffffUL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh+=(HASH_LONG)(len>>29);	/* might cause compiler warning on 16-bit */
	c->Nl = l;

	n = c->num;
	if (n != 0) {
		p = (unsigned char *)c->data;

		if (len >= HASH_CBLOCK || len + n >= HASH_CBLOCK) {
			memcpy (p + n, data, HASH_CBLOCK - n);
			HASH_BLOCK_DATA_ORDER (c, p, 1);
			n = HASH_CBLOCK - n;
			data += n;
			len -= n;
			c->num = 0;
			memset (p,0,HASH_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy (p + n, data, len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/HASH_CBLOCK;
	if (n > 0) {
		HASH_BLOCK_DATA_ORDER (c, data, n);
		n    *= HASH_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		p = (unsigned char *)c->data;
		c->num = (unsigned int)len;
		memcpy (p, data, len);
	}
	return 1;
}

void HASH_TRANSFORM (HASH_CTX *c, const unsigned char *data)
{
	HASH_BLOCK_DATA_ORDER (c, data, 1);
}

int HASH_FINAL (unsigned char *md, HASH_CTX *c)
{
	unsigned char *p = (unsigned char *)c->data;
	size_t n = c->num;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (HASH_CBLOCK - 8)) {
		memset (p + n, 0, HASH_CBLOCK - n);
		n = 0;
		HASH_BLOCK_DATA_ORDER (c, p, 1);
	}
	memset (p + n, 0, HASH_CBLOCK - 8 - n);

	p += HASH_CBLOCK - 8;
#if   defined(DATA_ORDER_IS_BIG_ENDIAN)
	HOST_l2c(c->Nh, p);
	HOST_l2c(c->Nl, p);
#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)
	HOST_l2c(c->Nl, p);
	HOST_l2c(c->Nh, p);
#endif
	p -= HASH_CBLOCK;
	HASH_BLOCK_DATA_ORDER (c, p, 1);
	c->num = 0;
	memset (p, 0, HASH_CBLOCK);

#ifndef HASH_MAKE_STRING
#error "HASH_MAKE_STRING must be defined!"
#else
	HASH_MAKE_STRING(c, md);
#endif

	return 1;
}

LCRYPTO_ALIAS(MD4_Update);
LCRYPTO_ALIAS(MD4_Final);
LCRYPTO_ALIAS(MD4_Transform);

/*
#define	F(x,y,z)	(((x) & (y))  |  ((~(x)) & (z)))
#define	G(x,y,z)	(((x) & (y))  |  ((x) & ((z))) | ((y) & ((z))))
*/

/* As pointed out by Wei Dai <weidai@eskimo.com>, the above can be
 * simplified to the code below.  Wei attributes these optimizations
 * to Peter Gutmann's SHS code, and he attributes it to Rich Schroeppel.
 */
#define	F(b,c,d)	((((c) ^ (d)) & (b)) ^ (d))
#define G(b,c,d)	(((b) & (c)) | ((b) & (d)) | ((c) & (d)))
#define	H(b,c,d)	((b) ^ (c) ^ (d))

#define R0(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+F((b),(c),(d))); \
	a=ROTATE(a,s); };

#define R1(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+G((b),(c),(d))); \
	a=ROTATE(a,s); };\

#define R2(a,b,c,d,k,s,t) { \
	a+=((k)+(t)+H((b),(c),(d))); \
	a=ROTATE(a,s); };

/* Implemented from RFC1186 The MD4 Message-Digest Algorithm
 */

#define INIT_DATA_A (unsigned long)0x67452301L
#define INIT_DATA_B (unsigned long)0xefcdab89L
#define INIT_DATA_C (unsigned long)0x98badcfeL
#define INIT_DATA_D (unsigned long)0x10325476L

#ifndef md4_block_data_order
#ifdef X
#undef X
#endif
void
md4_block_data_order(MD4_CTX *c, const void *data_, size_t num)
{
	const unsigned char *data = data_;
	unsigned int A, B, C, D, l;
	unsigned int X0, X1, X2, X3, X4, X5, X6, X7,
	    X8, X9, X10, X11, X12, X13, X14, X15;

	A = c->A;
	B = c->B;
	C = c->C;
	D = c->D;

	for (; num--; ) {
		HOST_c2l(data, l);
		X0 = l;
		HOST_c2l(data, l);
		X1 = l;
		/* Round 0 */
		R0(A, B, C, D, X0, 3, 0);
		HOST_c2l(data, l);
		X2 = l;
		R0(D, A, B, C, X1, 7, 0);
		HOST_c2l(data, l);
		X3 = l;
		R0(C, D, A, B, X2, 11, 0);
		HOST_c2l(data, l);
		X4 = l;
		R0(B, C, D, A, X3, 19, 0);
		HOST_c2l(data, l);
		X5 = l;
		R0(A, B, C, D, X4, 3, 0);
		HOST_c2l(data, l);
		X6 = l;
		R0(D, A, B, C, X5, 7, 0);
		HOST_c2l(data, l);
		X7 = l;
		R0(C, D, A, B, X6, 11, 0);
		HOST_c2l(data, l);
		X8 = l;
		R0(B, C, D, A, X7, 19, 0);
		HOST_c2l(data, l);
		X9 = l;
		R0(A, B, C, D, X8, 3, 0);
		HOST_c2l(data, l);
		X10 = l;
		R0(D, A,B, C,X9, 7, 0);
		HOST_c2l(data, l);
		X11 = l;
		R0(C, D,A, B,X10, 11, 0);
		HOST_c2l(data, l);
		X12 = l;
		R0(B, C,D, A,X11, 19, 0);
		HOST_c2l(data, l);
		X13 = l;
		R0(A, B,C, D,X12, 3, 0);
		HOST_c2l(data, l);
		X14 = l;
		R0(D, A,B, C,X13, 7, 0);
		HOST_c2l(data, l);
		X15 = l;
		R0(C, D,A, B,X14, 11, 0);
		R0(B, C,D, A,X15, 19, 0);
		/* Round 1 */
		R1(A, B, C, D, X0, 3, 0x5A827999L);
		R1(D, A, B, C, X4, 5, 0x5A827999L);
		R1(C, D, A, B, X8, 9, 0x5A827999L);
		R1(B, C, D, A, X12, 13, 0x5A827999L);
		R1(A, B, C, D, X1, 3, 0x5A827999L);
		R1(D, A, B, C, X5, 5, 0x5A827999L);
		R1(C, D, A, B, X9, 9, 0x5A827999L);
		R1(B, C, D, A, X13, 13, 0x5A827999L);
		R1(A, B, C, D, X2, 3, 0x5A827999L);
		R1(D, A, B, C, X6, 5, 0x5A827999L);
		R1(C, D, A, B, X10, 9, 0x5A827999L);
		R1(B, C, D, A, X14, 13, 0x5A827999L);
		R1(A, B, C, D, X3, 3, 0x5A827999L);
		R1(D, A, B, C, X7, 5, 0x5A827999L);
		R1(C, D, A, B, X11, 9, 0x5A827999L);
		R1(B, C, D, A, X15, 13, 0x5A827999L);
		/* Round 2 */
		R2(A, B, C, D, X0, 3, 0x6ED9EBA1L);
		R2(D, A, B, C, X8, 9, 0x6ED9EBA1L);
		R2(C, D, A, B, X4, 11, 0x6ED9EBA1L);
		R2(B, C, D, A, X12, 15, 0x6ED9EBA1L);
		R2(A, B, C, D, X2, 3, 0x6ED9EBA1L);
		R2(D, A, B, C, X10, 9, 0x6ED9EBA1L);
		R2(C, D, A, B, X6, 11, 0x6ED9EBA1L);
		R2(B, C, D, A, X14, 15, 0x6ED9EBA1L);
		R2(A, B, C, D, X1, 3, 0x6ED9EBA1L);
		R2(D, A, B, C, X9, 9, 0x6ED9EBA1L);
		R2(C, D, A, B, X5, 11, 0x6ED9EBA1L);
		R2(B, C, D, A, X13, 15, 0x6ED9EBA1L);
		R2(A, B, C, D, X3, 3, 0x6ED9EBA1L);
		R2(D, A, B, C, X11, 9, 0x6ED9EBA1L);
		R2(C, D, A, B, X7, 11, 0x6ED9EBA1L);
		R2(B, C, D, A, X15, 15, 0x6ED9EBA1L);

		A = c->A += A;
		B = c->B += B;
		C = c->C += C;
		D = c->D += D;
	}
}
#endif

int
MD4_Init(MD4_CTX *c)
{
	memset (c, 0, sizeof(*c));
	c->A = INIT_DATA_A;
	c->B = INIT_DATA_B;
	c->C = INIT_DATA_C;
	c->D = INIT_DATA_D;
	return 1;
}
LCRYPTO_ALIAS(MD4_Init);

unsigned char *
MD4(const unsigned char *d, size_t n, unsigned char *md)
{
	MD4_CTX c;
	static unsigned char m[MD4_DIGEST_LENGTH];

	if (md == NULL)
		md = m;
	if (!MD4_Init(&c))
		return NULL;
	MD4_Update(&c, d, n);
	MD4_Final(md, &c);
	explicit_bzero(&c, sizeof(c));
	return (md);
}
LCRYPTO_ALIAS(MD4);

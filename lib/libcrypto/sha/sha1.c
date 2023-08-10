/* $OpenBSD: sha1.c,v 1.12 2023/08/10 07:15:23 jsing Exp $ */
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

#include <stdlib.h>
#include <string.h>

#include <openssl/opensslconf.h>

#include <openssl/crypto.h>
#include <openssl/sha.h>

#if !defined(OPENSSL_NO_SHA1) && !defined(OPENSSL_NO_SHA)

#define DATA_ORDER_IS_BIG_ENDIAN

#define HASH_LONG               SHA_LONG
#define HASH_CTX                SHA_CTX
#define HASH_CBLOCK             SHA_CBLOCK

#define HASH_BLOCK_DATA_ORDER   	sha1_block_data_order
#define Xupdate(a, ix, ia, ib, ic, id)	( (a)=(ia^ib^ic^id),	\
					  ix=(a)=ROTATE((a),1)	\
					)

#ifndef SHA1_ASM
static
#endif
void sha1_block_data_order(SHA_CTX *c, const void *p, size_t num);

#define HASH_NO_UPDATE
#define HASH_NO_TRANSFORM
#define HASH_NO_FINAL

#include "md32_common.h"

#define K_00_19	0x5a827999UL
#define K_20_39 0x6ed9eba1UL
#define K_40_59 0x8f1bbcdcUL
#define K_60_79 0xca62c1d6UL

/* As  pointed out by Wei Dai <weidai@eskimo.com>, F() below can be
 * simplified to the code in F_00_19.  Wei attributes these optimisations
 * to Peter Gutmann's SHS code, and he attributes it to Rich Schroeppel.
 * #define F(x,y,z) (((x) & (y))  |  ((~(x)) & (z)))
 * I've just become aware of another tweak to be made, again from Wei Dai,
 * in F_40_59, (x&a)|(y&a) -> (x|y)&a
 */
#define	F_00_19(b, c, d)	((((c) ^ (d)) & (b)) ^ (d))
#define	F_20_39(b, c, d)	((b) ^ (c) ^ (d))
#define F_40_59(b, c, d)	(((b) & (c)) | (((b)|(c)) & (d)))
#define	F_60_79(b, c, d)	F_20_39(b, c, d)


#define BODY_00_15(i, a, b, c, d, e, f, xi) \
	(f)=xi+(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_16_19(i, a, b, c, d, e, f, xi, xa, xb, xc, xd) \
	Xupdate(f, xi, xa, xb, xc, xd); \
	(f)+=(e)+K_00_19+ROTATE((a),5)+F_00_19((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_20_31(i, a, b, c, d, e, f, xi, xa, xb, xc, xd) \
	Xupdate(f, xi, xa, xb, xc, xd); \
	(f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_32_39(i, a, b, c, d, e, f, xa, xb, xc, xd) \
	Xupdate(f, xa, xa, xb, xc, xd); \
	(f)+=(e)+K_20_39+ROTATE((a),5)+F_20_39((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_40_59(i, a, b, c, d, e, f, xa, xb, xc, xd) \
	Xupdate(f, xa, xa, xb, xc, xd); \
	(f)+=(e)+K_40_59+ROTATE((a),5)+F_40_59((b),(c),(d)); \
	(b)=ROTATE((b),30);

#define BODY_60_79(i, a, b, c, d, e, f, xa, xb, xc, xd) \
	Xupdate(f, xa, xa, xb, xc, xd); \
	(f)=xa+(e)+K_60_79+ROTATE((a),5)+F_60_79((b),(c),(d)); \
	(b)=ROTATE((b),30);

#if !defined(SHA1_ASM)
#include <endian.h>
static void
sha1_block_data_order(SHA_CTX *c, const void *p, size_t num)
{
	const unsigned char *data = p;
	unsigned int A, B, C, D, E, T, l;
	unsigned int X0, X1, X2, X3, X4, X5, X6, X7,
	    X8, X9, X10, X11, X12, X13, X14, X15;

	A = c->h0;
	B = c->h1;
	C = c->h2;
	D = c->h3;
	E = c->h4;

	for (;;) {

		if (BYTE_ORDER != LITTLE_ENDIAN &&
		    sizeof(SHA_LONG) == 4 && ((size_t)p % 4) == 0) {
			const SHA_LONG *W = (const SHA_LONG *)data;

			X0 = W[0];
			X1 = W[1];
			BODY_00_15( 0, A, B, C, D, E, T, X0);
			X2 = W[2];
			BODY_00_15( 1, T, A, B, C, D, E, X1);
			X3 = W[3];
			BODY_00_15( 2, E, T, A, B, C, D, X2);
			X4 = W[4];
			BODY_00_15( 3, D, E, T, A, B, C, X3);
			X5 = W[5];
			BODY_00_15( 4, C, D, E, T, A, B, X4);
			X6 = W[6];
			BODY_00_15( 5, B, C, D, E, T, A, X5);
			X7 = W[7];
			BODY_00_15( 6, A, B, C, D, E, T, X6);
			X8 = W[8];
			BODY_00_15( 7, T, A, B, C, D, E, X7);
			X9 = W[9];
			BODY_00_15( 8, E, T, A, B, C, D, X8);
			X10 = W[10];
			BODY_00_15( 9, D, E, T, A, B, C, X9);
			X11 = W[11];
			BODY_00_15(10, C, D, E, T, A, B, X10);
			X12 = W[12];
			BODY_00_15(11, B, C, D, E, T, A, X11);
			X13 = W[13];
			BODY_00_15(12, A, B, C, D, E, T, X12);
			X14 = W[14];
			BODY_00_15(13, T, A, B, C, D, E, X13);
			X15 = W[15];
			BODY_00_15(14, E, T, A, B, C, D, X14);
			BODY_00_15(15, D, E, T, A, B, C, X15);

			data += SHA_CBLOCK;
		} else {
			HOST_c2l(data, l);
			X0 = l;
			HOST_c2l(data, l);
			X1 = l;
			BODY_00_15( 0, A, B, C, D, E, T, X0);
			HOST_c2l(data, l);
			X2 = l;
			BODY_00_15( 1, T, A, B, C, D, E, X1);
			HOST_c2l(data, l);
			X3 = l;
			BODY_00_15( 2, E, T, A, B, C, D, X2);
			HOST_c2l(data, l);
			X4 = l;
			BODY_00_15( 3, D, E, T, A, B, C, X3);
			HOST_c2l(data, l);
			X5 = l;
			BODY_00_15( 4, C, D, E, T, A, B, X4);
			HOST_c2l(data, l);
			X6 = l;
			BODY_00_15( 5, B, C, D, E, T, A, X5);
			HOST_c2l(data, l);
			X7 = l;
			BODY_00_15( 6, A, B, C, D, E, T, X6);
			HOST_c2l(data, l);
			X8 = l;
			BODY_00_15( 7, T, A, B, C, D, E, X7);
			HOST_c2l(data, l);
			X9 = l;
			BODY_00_15( 8, E, T, A, B, C, D, X8);
			HOST_c2l(data, l);
			X10 = l;
			BODY_00_15( 9, D, E, T, A, B, C, X9);
			HOST_c2l(data, l);
			X11 = l;
			BODY_00_15(10, C, D, E, T, A, B, X10);
			HOST_c2l(data, l);
			X12 = l;
			BODY_00_15(11, B, C, D, E, T, A, X11);
			HOST_c2l(data, l);
			X13 = l;
			BODY_00_15(12, A, B, C, D, E, T, X12);
			HOST_c2l(data, l);
			X14 = l;
			BODY_00_15(13, T, A, B, C, D, E, X13);
			HOST_c2l(data, l);
			X15 = l;
			BODY_00_15(14, E, T, A, B, C, D, X14);
			BODY_00_15(15, D, E, T, A, B, C, X15);
		}

		BODY_16_19(16, C, D, E, T, A, B, X0, X0, X2, X8, X13);
		BODY_16_19(17, B, C, D, E, T, A, X1, X1, X3, X9, X14);
		BODY_16_19(18, A, B, C, D, E, T, X2, X2, X4, X10, X15);
		BODY_16_19(19, T, A, B, C, D, E, X3, X3, X5, X11, X0);

		BODY_20_31(20, E, T, A, B, C, D, X4, X4, X6, X12, X1);
		BODY_20_31(21, D, E, T, A, B, C, X5, X5, X7, X13, X2);
		BODY_20_31(22, C, D, E, T, A, B, X6, X6, X8, X14, X3);
		BODY_20_31(23, B, C, D, E, T, A, X7, X7, X9, X15, X4);
		BODY_20_31(24, A, B, C, D, E, T, X8, X8, X10, X0, X5);
		BODY_20_31(25, T, A, B, C, D, E, X9, X9, X11, X1, X6);
		BODY_20_31(26, E, T, A, B, C, D, X10, X10, X12, X2, X7);
		BODY_20_31(27, D, E, T, A, B, C, X11, X11, X13, X3, X8);
		BODY_20_31(28, C, D, E, T, A, B, X12, X12, X14, X4, X9);
		BODY_20_31(29, B, C, D, E, T, A, X13, X13, X15, X5, X10);
		BODY_20_31(30, A, B, C, D, E, T, X14, X14, X0, X6, X11);
		BODY_20_31(31, T, A, B, C, D, E, X15, X15, X1, X7, X12);

		BODY_32_39(32, E, T, A, B, C, D, X0, X2, X8, X13);
		BODY_32_39(33, D, E, T, A, B, C, X1, X3, X9, X14);
		BODY_32_39(34, C, D, E, T, A, B, X2, X4, X10, X15);
		BODY_32_39(35, B, C, D, E, T, A, X3, X5, X11, X0);
		BODY_32_39(36, A, B, C, D, E, T, X4, X6, X12, X1);
		BODY_32_39(37, T, A, B, C, D, E, X5, X7, X13, X2);
		BODY_32_39(38, E, T, A, B, C, D, X6, X8, X14, X3);
		BODY_32_39(39, D, E, T, A, B, C, X7, X9, X15, X4);

		BODY_40_59(40, C, D, E, T, A, B, X8, X10, X0, X5);
		BODY_40_59(41, B, C, D, E, T, A, X9, X11, X1, X6);
		BODY_40_59(42, A, B, C, D, E, T, X10, X12, X2, X7);
		BODY_40_59(43, T, A, B, C, D, E, X11, X13, X3, X8);
		BODY_40_59(44, E, T, A, B, C, D, X12, X14, X4, X9);
		BODY_40_59(45, D, E, T, A, B, C, X13, X15, X5, X10);
		BODY_40_59(46, C, D, E, T, A, B, X14, X0, X6, X11);
		BODY_40_59(47, B, C, D, E, T, A, X15, X1, X7, X12);
		BODY_40_59(48, A, B, C, D, E, T, X0, X2, X8, X13);
		BODY_40_59(49, T, A, B, C, D, E, X1, X3, X9, X14);
		BODY_40_59(50, E, T, A, B, C, D, X2, X4, X10, X15);
		BODY_40_59(51, D, E, T, A, B, C, X3, X5, X11, X0);
		BODY_40_59(52, C, D, E, T, A, B, X4, X6, X12, X1);
		BODY_40_59(53, B, C, D, E, T, A, X5, X7, X13, X2);
		BODY_40_59(54, A, B, C, D, E, T, X6, X8, X14, X3);
		BODY_40_59(55, T, A, B, C, D, E, X7, X9, X15, X4);
		BODY_40_59(56, E, T, A, B, C, D, X8, X10, X0, X5);
		BODY_40_59(57, D, E, T, A, B, C, X9, X11, X1, X6);
		BODY_40_59(58, C, D, E, T, A, B, X10, X12, X2, X7);
		BODY_40_59(59, B, C, D, E, T, A, X11, X13, X3, X8);

		BODY_60_79(60, A, B, C, D, E, T, X12, X14, X4, X9);
		BODY_60_79(61, T, A, B, C, D, E, X13, X15, X5, X10);
		BODY_60_79(62, E, T, A, B, C, D, X14, X0, X6, X11);
		BODY_60_79(63, D, E, T, A, B, C, X15, X1, X7, X12);
		BODY_60_79(64, C, D, E, T, A, B, X0, X2, X8, X13);
		BODY_60_79(65, B, C, D, E, T, A, X1, X3, X9, X14);
		BODY_60_79(66, A, B, C, D, E, T, X2, X4, X10, X15);
		BODY_60_79(67, T, A, B, C, D, E, X3, X5, X11, X0);
		BODY_60_79(68, E, T, A, B, C, D, X4, X6, X12, X1);
		BODY_60_79(69, D, E, T, A, B, C, X5, X7, X13, X2);
		BODY_60_79(70, C, D, E, T, A, B, X6, X8, X14, X3);
		BODY_60_79(71, B, C, D, E, T, A, X7, X9, X15, X4);
		BODY_60_79(72, A, B, C, D, E, T, X8, X10, X0, X5);
		BODY_60_79(73, T, A, B, C, D, E, X9, X11, X1, X6);
		BODY_60_79(74, E, T, A, B, C, D, X10, X12, X2, X7);
		BODY_60_79(75, D, E, T, A, B, C, X11, X13, X3, X8);
		BODY_60_79(76, C, D, E, T, A, B, X12, X14, X4, X9);
		BODY_60_79(77, B, C, D, E, T, A, X13, X15, X5, X10);
		BODY_60_79(78, A, B, C, D, E, T, X14, X0, X6, X11);
		BODY_60_79(79, T, A, B, C, D, E, X15, X1, X7, X12);

		c->h0 = (c->h0 + E)&0xffffffffL;
		c->h1 = (c->h1 + T)&0xffffffffL;
		c->h2 = (c->h2 + A)&0xffffffffL;
		c->h3 = (c->h3 + B)&0xffffffffL;
		c->h4 = (c->h4 + C)&0xffffffffL;

		if (--num == 0)
			break;

		A = c->h0;
		B = c->h1;
		C = c->h2;
		D = c->h3;
		E = c->h4;

	}
}
#endif


int
SHA1_Init(SHA_CTX *c)
{
	memset(c, 0, sizeof(*c));

	c->h0 = 0x67452301UL;
	c->h1 = 0xefcdab89UL;
	c->h2 = 0x98badcfeUL;
	c->h3 = 0x10325476UL;
	c->h4 = 0xc3d2e1f0UL;

	return 1;
}
LCRYPTO_ALIAS(SHA1_Init);

int
SHA1_Update(SHA_CTX *c, const void *data_, size_t len)
{
	const unsigned char *data = data_;
	unsigned char *p;
	SHA_LONG l;
	size_t n;

	if (len == 0)
		return 1;

	l = (c->Nl + (((SHA_LONG)len) << 3))&0xffffffffUL;
	/* 95-05-24 eay Fixed a bug with the overflow handling, thanks to
	 * Wei Dai <weidai@eskimo.com> for pointing it out. */
	if (l < c->Nl) /* overflow */
		c->Nh++;
	c->Nh+=(SHA_LONG)(len>>29);	/* might cause compiler warning on 16-bit */
	c->Nl = l;

	n = c->num;
	if (n != 0) {
		p = (unsigned char *)c->data;

		if (len >= SHA_CBLOCK || len + n >= SHA_CBLOCK) {
			memcpy(p + n, data, SHA_CBLOCK - n);
			sha1_block_data_order(c, p, 1);
			n = SHA_CBLOCK - n;
			data += n;
			len -= n;
			c->num = 0;
			memset(p,0,SHA_CBLOCK);	/* keep it zeroed */
		} else {
			memcpy(p + n, data, len);
			c->num += (unsigned int)len;
			return 1;
		}
	}

	n = len/SHA_CBLOCK;
	if (n > 0) {
		sha1_block_data_order(c, data, n);
		n    *= SHA_CBLOCK;
		data += n;
		len -= n;
	}

	if (len != 0) {
		p = (unsigned char *)c->data;
		c->num = (unsigned int)len;
		memcpy(p, data, len);
	}
	return 1;
}
LCRYPTO_ALIAS(SHA1_Update);

void
SHA1_Transform(SHA_CTX *c, const unsigned char *data)
{
	sha1_block_data_order(c, data, 1);
}
LCRYPTO_ALIAS(SHA1_Transform);

int
SHA1_Final(unsigned char *md, SHA_CTX *c)
{
	unsigned char *p = (unsigned char *)c->data;
	unsigned long ll;
	size_t n = c->num;

	p[n] = 0x80; /* there is always room for one */
	n++;

	if (n > (SHA_CBLOCK - 8)) {
		memset(p + n, 0, SHA_CBLOCK - n);
		n = 0;
		sha1_block_data_order(c, p, 1);
	}
	memset(p + n, 0, SHA_CBLOCK - 8 - n);

	p += SHA_CBLOCK - 8;
#if   defined(DATA_ORDER_IS_BIG_ENDIAN)
	HOST_l2c(c->Nh, p);
	HOST_l2c(c->Nl, p);
#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)
	HOST_l2c(c->Nl, p);
	HOST_l2c(c->Nh, p);
#endif
	p -= SHA_CBLOCK;
	sha1_block_data_order(c, p, 1);
	c->num = 0;
	memset(p, 0, SHA_CBLOCK);

	ll = c->h0;
	HOST_l2c(ll, md);
	ll = c->h1;
	HOST_l2c(ll, md);
	ll = c->h2;
	HOST_l2c(ll, md);
	ll = c->h3;
	HOST_l2c(ll, md);
	ll = c->h4;
	HOST_l2c(ll, md);

	return 1;
}
LCRYPTO_ALIAS(SHA1_Final);

unsigned char *
SHA1(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA_CTX c;
	static unsigned char m[SHA_DIGEST_LENGTH];

	if (md == NULL)
		md = m;

	if (!SHA1_Init(&c))
		return NULL;
	SHA1_Update(&c, d, n);
	SHA1_Final(md, &c);

	explicit_bzero(&c, sizeof(c));

	return (md);
}
LCRYPTO_ALIAS(SHA1);

#endif

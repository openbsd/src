/* $OpenBSD: ripemd.c,v 1.7 2023/08/10 12:27:35 jsing Exp $ */
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
#include <openssl/opensslv.h>
#include <openssl/crypto.h>

#include <stdlib.h>
#include <string.h>
#include <openssl/opensslconf.h>
#include <openssl/ripemd.h>

/*
 * DO EXAMINE COMMENTS IN crypto/md5/md5_locl.h & crypto/md5/md5_dgst.c
 * FOR EXPLANATIONS ON FOLLOWING "CODE."
 *					<appro@fy.chalmers.se>
 */
#ifdef RMD160_ASM
# if defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(__INTEL__)
#  define ripemd160_block_data_order ripemd160_block_asm_data_order
# endif
#endif

__BEGIN_HIDDEN_DECLS

void ripemd160_block_data_order (RIPEMD160_CTX *c, const void *p, size_t num);

__END_HIDDEN_DECLS

#define DATA_ORDER_IS_LITTLE_ENDIAN

#define HASH_LONG               RIPEMD160_LONG
#define HASH_CTX                RIPEMD160_CTX
#define HASH_CBLOCK             RIPEMD160_CBLOCK
#define HASH_UPDATE             RIPEMD160_Update
#define HASH_TRANSFORM          RIPEMD160_Transform
#define HASH_FINAL              RIPEMD160_Final
#define	HASH_MAKE_STRING(c,s)	do {	\
	unsigned long ll;		\
	ll=(c)->A; HOST_l2c(ll,(s));	\
	ll=(c)->B; HOST_l2c(ll,(s));	\
	ll=(c)->C; HOST_l2c(ll,(s));	\
	ll=(c)->D; HOST_l2c(ll,(s));	\
	ll=(c)->E; HOST_l2c(ll,(s));	\
	} while (0)
#define HASH_BLOCK_DATA_ORDER   ripemd160_block_data_order

#include "md32_common.h"

#if 0
#define F1(x,y,z)	 ((x)^(y)^(z))
#define F2(x,y,z)	(((x)&(y))|((~x)&z))
#define F3(x,y,z)	(((x)|(~y))^(z))
#define F4(x,y,z)	(((x)&(z))|((y)&(~(z))))
#define F5(x,y,z)	 ((x)^((y)|(~(z))))
#else
/*
 * Transformed F2 and F4 are courtesy of Wei Dai <weidai@eskimo.com>
 */
#define F1(x,y,z)	((x) ^ (y) ^ (z))
#define F2(x,y,z)	((((y) ^ (z)) & (x)) ^ (z))
#define F3(x,y,z)	(((~(y)) | (x)) ^ (z))
#define F4(x,y,z)	((((x) ^ (y)) & (z)) ^ (y))
#define F5(x,y,z)	(((~(z)) | (y)) ^ (x))
#endif

#define RIPEMD160_A	0x67452301L
#define RIPEMD160_B	0xEFCDAB89L
#define RIPEMD160_C	0x98BADCFEL
#define RIPEMD160_D	0x10325476L
#define RIPEMD160_E	0xC3D2E1F0L

#define KL0 0x00000000L
#define KL1 0x5A827999L
#define KL2 0x6ED9EBA1L
#define KL3 0x8F1BBCDCL
#define KL4 0xA953FD4EL

#define KR0 0x50A28BE6L
#define KR1 0x5C4DD124L
#define KR2 0x6D703EF3L
#define KR3 0x7A6D76E9L
#define KR4 0x00000000L

#define RIP1(a,b,c,d,e,w,s) { \
	a+=F1(b,c,d)+w; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP2(a,b,c,d,e,w,s,K) { \
	a+=F2(b,c,d)+w+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP3(a,b,c,d,e,w,s,K) { \
	a+=F3(b,c,d)+w+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP4(a,b,c,d,e,w,s,K) { \
	a+=F4(b,c,d)+w+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP5(a,b,c,d,e,w,s,K) { \
	a+=F5(b,c,d)+w+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#  ifdef RMD160_ASM
void ripemd160_block_x86(RIPEMD160_CTX *c, unsigned long *p, size_t num);
#    define ripemd160_block ripemd160_block_x86
#  else
void ripemd160_block(RIPEMD160_CTX *c, unsigned long *p, size_t num);
#  endif

int
RIPEMD160_Init(RIPEMD160_CTX *c)
{
	memset (c, 0, sizeof(*c));
	c->A = RIPEMD160_A;
	c->B = RIPEMD160_B;
	c->C = RIPEMD160_C;
	c->D = RIPEMD160_D;
	c->E = RIPEMD160_E;
	return 1;
}

#ifndef ripemd160_block_data_order
void
ripemd160_block_data_order(RIPEMD160_CTX *ctx, const void *p, size_t num)
{
	const unsigned char *data = p;
	unsigned int A, B, C, D, E;
	unsigned int a, b, c, d, e, l;
	unsigned int X0, X1, X2, X3, X4, X5, X6, X7,
	    X8, X9, X10, X11, X12, X13, X14, X15;

	for (; num--; ) {

		A = ctx->A;
		B = ctx->B;
		C = ctx->C;
		D = ctx->D;
		E = ctx->E;

		HOST_c2l(data, l);
		X0 = l;
		HOST_c2l(data, l);
		X1 = l;
		RIP1(A, B, C, D, E, X0, 11);
		HOST_c2l(data, l);
		X2 = l;
		RIP1(E, A, B, C, D, X1, 14);
		HOST_c2l(data, l);
		X3 = l;
		RIP1(D, E, A, B, C, X2, 15);
		HOST_c2l(data, l);
		X4 = l;
		RIP1(C, D, E, A, B, X3, 12);
		HOST_c2l(data, l);
		X5 = l;
		RIP1(B, C, D, E, A, X4, 5);
		HOST_c2l(data, l);
		X6 = l;
		RIP1(A, B, C, D, E, X5, 8);
		HOST_c2l(data, l);
		X7 = l;
		RIP1(E, A, B, C, D, X6, 7);
		HOST_c2l(data, l);
		X8 = l;
		RIP1(D, E, A, B, C, X7, 9);
		HOST_c2l(data, l);
		X9 = l;
		RIP1(C, D, E, A, B, X8, 11);
		HOST_c2l(data, l);
		X10 = l;
		RIP1(B, C, D, E, A, X9, 13);
		HOST_c2l(data, l);
		X11 = l;
		RIP1(A, B, C, D, E, X10, 14);
		HOST_c2l(data, l);
		X12 = l;
		RIP1(E, A, B, C, D, X11, 15);
		HOST_c2l(data, l);
		X13 = l;
		RIP1(D, E, A, B, C, X12, 6);
		HOST_c2l(data, l);
		X14 = l;
		RIP1(C, D, E, A, B, X13, 7);
		HOST_c2l(data, l);
		X15 = l;
		RIP1(B, C, D, E, A, X14, 9);
		RIP1(A, B, C, D, E, X15, 8);

		RIP2(E, A, B, C, D, X7, 7, KL1);
		RIP2(D, E, A, B, C, X4, 6, KL1);
		RIP2(C, D, E, A, B, X13, 8, KL1);
		RIP2(B, C, D, E, A, X1, 13, KL1);
		RIP2(A, B, C, D, E, X10, 11, KL1);
		RIP2(E, A, B, C, D, X6, 9, KL1);
		RIP2(D, E, A, B, C, X15, 7, KL1);
		RIP2(C, D, E, A, B, X3, 15, KL1);
		RIP2(B, C, D, E, A, X12, 7, KL1);
		RIP2(A, B, C, D, E, X0, 12, KL1);
		RIP2(E, A, B, C, D, X9, 15, KL1);
		RIP2(D, E, A, B, C, X5, 9, KL1);
		RIP2(C, D, E, A, B, X2, 11, KL1);
		RIP2(B, C, D, E, A, X14, 7, KL1);
		RIP2(A, B, C, D, E, X11, 13, KL1);
		RIP2(E, A, B, C, D, X8, 12, KL1);

		RIP3(D, E, A, B, C, X3, 11, KL2);
		RIP3(C, D, E, A, B, X10, 13, KL2);
		RIP3(B, C, D, E, A, X14, 6, KL2);
		RIP3(A, B, C, D, E, X4, 7, KL2);
		RIP3(E, A, B, C, D, X9, 14, KL2);
		RIP3(D, E, A, B, C, X15, 9, KL2);
		RIP3(C, D, E, A, B, X8, 13, KL2);
		RIP3(B, C, D, E, A, X1, 15, KL2);
		RIP3(A, B, C, D, E, X2, 14, KL2);
		RIP3(E, A, B, C, D, X7, 8, KL2);
		RIP3(D, E, A, B, C, X0, 13, KL2);
		RIP3(C, D, E, A, B, X6, 6, KL2);
		RIP3(B, C, D, E, A, X13, 5, KL2);
		RIP3(A, B, C, D, E, X11, 12, KL2);
		RIP3(E, A, B, C, D, X5, 7, KL2);
		RIP3(D, E, A, B, C, X12, 5, KL2);

		RIP4(C, D, E, A, B, X1, 11, KL3);
		RIP4(B, C, D, E, A, X9, 12, KL3);
		RIP4(A, B, C, D, E, X11, 14, KL3);
		RIP4(E, A, B, C, D, X10, 15, KL3);
		RIP4(D, E, A, B, C, X0, 14, KL3);
		RIP4(C, D, E, A, B, X8, 15, KL3);
		RIP4(B, C, D, E, A, X12, 9, KL3);
		RIP4(A, B, C, D, E, X4, 8, KL3);
		RIP4(E, A, B, C, D, X13, 9, KL3);
		RIP4(D, E, A, B, C, X3, 14, KL3);
		RIP4(C, D, E, A, B, X7, 5, KL3);
		RIP4(B, C, D, E, A, X15, 6, KL3);
		RIP4(A, B, C, D, E, X14, 8, KL3);
		RIP4(E, A, B, C, D, X5, 6, KL3);
		RIP4(D, E, A, B, C, X6, 5, KL3);
		RIP4(C, D, E, A, B, X2, 12, KL3);

		RIP5(B, C, D, E, A, X4, 9, KL4);
		RIP5(A, B, C, D, E, X0, 15, KL4);
		RIP5(E, A, B, C, D, X5, 5, KL4);
		RIP5(D, E, A, B, C, X9, 11, KL4);
		RIP5(C, D, E, A, B, X7, 6, KL4);
		RIP5(B, C, D, E, A, X12, 8, KL4);
		RIP5(A, B, C, D, E, X2, 13, KL4);
		RIP5(E, A, B, C, D, X10, 12, KL4);
		RIP5(D, E, A, B, C, X14, 5, KL4);
		RIP5(C, D, E, A, B, X1, 12, KL4);
		RIP5(B, C, D, E, A, X3, 13, KL4);
		RIP5(A, B, C, D, E, X8, 14, KL4);
		RIP5(E, A, B, C, D, X11, 11, KL4);
		RIP5(D, E, A, B, C, X6, 8, KL4);
		RIP5(C, D, E, A, B, X15, 5, KL4);
		RIP5(B, C, D, E, A, X13, 6, KL4);

		a = A;
		b = B;
		c = C;
		d = D;
		e = E;
		/* Do other half */
		A = ctx->A;
		B = ctx->B;
		C = ctx->C;
		D = ctx->D;
		E = ctx->E;

		RIP5(A, B, C, D, E, X5, 8, KR0);
		RIP5(E, A, B, C, D, X14, 9, KR0);
		RIP5(D, E, A, B, C, X7, 9, KR0);
		RIP5(C, D, E, A, B, X0, 11, KR0);
		RIP5(B, C, D, E, A, X9, 13, KR0);
		RIP5(A, B, C, D, E, X2, 15, KR0);
		RIP5(E, A, B, C, D, X11, 15, KR0);
		RIP5(D, E, A, B, C, X4, 5, KR0);
		RIP5(C, D, E, A, B, X13, 7, KR0);
		RIP5(B, C, D, E, A, X6, 7, KR0);
		RIP5(A, B, C, D, E, X15, 8, KR0);
		RIP5(E, A, B, C, D, X8, 11, KR0);
		RIP5(D, E, A, B, C, X1, 14, KR0);
		RIP5(C, D, E, A, B, X10, 14, KR0);
		RIP5(B, C, D, E, A, X3, 12, KR0);
		RIP5(A, B, C, D, E, X12, 6, KR0);

		RIP4(E, A, B, C, D, X6, 9, KR1);
		RIP4(D, E, A, B, C, X11, 13, KR1);
		RIP4(C, D, E, A, B, X3, 15, KR1);
		RIP4(B, C, D, E, A, X7, 7, KR1);
		RIP4(A, B, C, D, E, X0, 12, KR1);
		RIP4(E, A, B, C, D, X13, 8, KR1);
		RIP4(D, E, A, B, C, X5, 9, KR1);
		RIP4(C, D, E, A, B, X10, 11, KR1);
		RIP4(B, C, D, E, A, X14, 7, KR1);
		RIP4(A, B, C, D, E, X15, 7, KR1);
		RIP4(E, A, B, C, D, X8, 12, KR1);
		RIP4(D, E, A, B, C, X12, 7, KR1);
		RIP4(C, D, E, A, B, X4, 6, KR1);
		RIP4(B, C, D, E, A, X9, 15, KR1);
		RIP4(A, B, C, D, E, X1, 13, KR1);
		RIP4(E, A, B, C, D, X2, 11, KR1);

		RIP3(D, E, A, B, C, X15, 9, KR2);
		RIP3(C, D, E, A, B, X5, 7, KR2);
		RIP3(B, C, D, E, A, X1, 15, KR2);
		RIP3(A, B, C, D, E, X3, 11, KR2);
		RIP3(E, A, B, C, D, X7, 8, KR2);
		RIP3(D, E, A, B, C, X14, 6, KR2);
		RIP3(C, D, E, A, B, X6, 6, KR2);
		RIP3(B, C, D, E, A, X9, 14, KR2);
		RIP3(A, B, C, D, E, X11, 12, KR2);
		RIP3(E, A, B, C, D, X8, 13, KR2);
		RIP3(D, E, A, B, C, X12, 5, KR2);
		RIP3(C, D, E, A, B, X2, 14, KR2);
		RIP3(B, C, D, E, A, X10, 13, KR2);
		RIP3(A, B, C, D, E, X0, 13, KR2);
		RIP3(E, A, B, C, D, X4, 7, KR2);
		RIP3(D, E, A, B, C, X13, 5, KR2);

		RIP2(C, D, E, A, B, X8, 15, KR3);
		RIP2(B, C, D, E, A, X6, 5, KR3);
		RIP2(A, B, C, D, E, X4, 8, KR3);
		RIP2(E, A, B, C, D, X1, 11, KR3);
		RIP2(D, E, A, B, C, X3, 14, KR3);
		RIP2(C, D, E, A, B, X11, 14, KR3);
		RIP2(B, C, D, E, A, X15, 6, KR3);
		RIP2(A, B, C, D, E, X0, 14, KR3);
		RIP2(E, A, B, C, D, X5, 6, KR3);
		RIP2(D, E, A, B, C, X12, 9, KR3);
		RIP2(C, D, E, A, B, X2, 12, KR3);
		RIP2(B, C, D, E, A, X13, 9, KR3);
		RIP2(A, B, C, D, E, X9, 12, KR3);
		RIP2(E, A, B, C, D, X7, 5, KR3);
		RIP2(D, E, A, B, C, X10, 15, KR3);
		RIP2(C, D, E, A, B, X14, 8, KR3);

		RIP1(B, C, D, E, A, X12, 8);
		RIP1(A, B, C, D, E, X15, 5);
		RIP1(E, A, B, C, D, X10, 12);
		RIP1(D, E, A, B, C, X4, 9);
		RIP1(C, D, E, A, B, X1, 12);
		RIP1(B, C, D, E, A, X5, 5);
		RIP1(A, B, C, D, E, X8, 14);
		RIP1(E, A, B, C, D, X7, 6);
		RIP1(D, E, A, B, C, X6, 8);
		RIP1(C, D, E, A, B, X2, 13);
		RIP1(B, C, D, E, A, X13, 6);
		RIP1(A, B, C, D, E, X14, 5);
		RIP1(E, A, B, C, D, X0, 15);
		RIP1(D, E, A, B, C, X3, 13);
		RIP1(C, D, E, A, B, X9, 11);
		RIP1(B, C, D, E, A, X11, 11);

		D = ctx->B + c + D;
		ctx->B = ctx->C + d + E;
		ctx->C = ctx->D + e + A;
		ctx->D = ctx->E + a + B;
		ctx->E = ctx->A + b + C;
		ctx->A = D;

	}
}
#endif

unsigned char *
RIPEMD160(const unsigned char *d, size_t n,
    unsigned char *md)
{
	RIPEMD160_CTX c;
	static unsigned char m[RIPEMD160_DIGEST_LENGTH];

	if (md == NULL)
		md = m;
	if (!RIPEMD160_Init(&c))
		return NULL;
	RIPEMD160_Update(&c, d, n);
	RIPEMD160_Final(md, &c);
	explicit_bzero(&c, sizeof(c));
	return (md);
}

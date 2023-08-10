/* $OpenBSD: ripemd.c,v 1.4 2023/08/10 11:00:46 jsing Exp $ */
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

#include "rmdconst.h"

#define RIP1(a,b,c,d,e,w,s) { \
	a+=F1(b,c,d)+X(w); \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP2(a,b,c,d,e,w,s,K) { \
	a+=F2(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP3(a,b,c,d,e,w,s,K) { \
	a+=F3(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP4(a,b,c,d,e,w,s,K) { \
	a+=F4(b,c,d)+X(w)+K; \
        a=ROTATE(a,s)+e; \
        c=ROTATE(c,10); }

#define RIP5(a,b,c,d,e,w,s,K) { \
	a+=F5(b,c,d)+X(w)+K; \
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
#ifdef X
#undef X
#endif
void
ripemd160_block_data_order(RIPEMD160_CTX *ctx, const void *p, size_t num)
{
	const unsigned char *data = p;
	unsigned int A, B, C, D, E;
	unsigned int a, b, c, d, e, l;
#ifndef MD32_XARRAY
	/* See comment in crypto/sha/sha_locl.h for details. */
	unsigned int XX0, XX1, XX2, XX3, XX4, XX5, XX6, XX7,
	    XX8, XX9, XX10, XX11, XX12, XX13, XX14, XX15;
# define X(i)	XX##i
#else
	RIPEMD160_LONG	XX[16];
# define X(i)	XX[i]
#endif

	for (; num--; ) {

		A = ctx->A;
		B = ctx->B;
		C = ctx->C;
		D = ctx->D;
		E = ctx->E;

		HOST_c2l(data, l);
		X( 0) = l;
		HOST_c2l(data, l);
		X( 1) = l;
		RIP1(A, B, C, D, E, WL00, 11);
		HOST_c2l(data, l);
		X( 2) = l;
		RIP1(E, A, B, C, D, WL01, 14);
		HOST_c2l(data, l);
		X( 3) = l;
		RIP1(D, E, A, B, C, WL02, 15);
		HOST_c2l(data, l);
		X( 4) = l;
		RIP1(C, D, E, A, B, WL03, 12);
		HOST_c2l(data, l);
		X( 5) = l;
		RIP1(B, C, D, E, A, WL04, 5);
		HOST_c2l(data, l);
		X( 6) = l;
		RIP1(A, B, C, D, E, WL05, 8);
		HOST_c2l(data, l);
		X( 7) = l;
		RIP1(E, A, B, C, D, WL06, 7);
		HOST_c2l(data, l);
		X( 8) = l;
		RIP1(D, E, A, B, C, WL07, 9);
		HOST_c2l(data, l);
		X( 9) = l;
		RIP1(C, D, E, A, B, WL08, 11);
		HOST_c2l(data, l);
		X(10) = l;
		RIP1(B, C, D, E, A, WL09, 13);
		HOST_c2l(data, l);
		X(11) = l;
		RIP1(A, B, C, D, E, WL10, 14);
		HOST_c2l(data, l);
		X(12) = l;
		RIP1(E, A, B, C, D, WL11, 15);
		HOST_c2l(data, l);
		X(13) = l;
		RIP1(D, E, A, B, C, WL12, 6);
		HOST_c2l(data, l);
		X(14) = l;
		RIP1(C, D, E, A, B, WL13, 7);
		HOST_c2l(data, l);
		X(15) = l;
		RIP1(B, C, D, E, A, WL14, 9);
		RIP1(A, B, C, D, E, WL15, 8);

		RIP2(E, A, B, C, D, WL16, 7, KL1);
		RIP2(D, E, A, B, C, WL17, 6, KL1);
		RIP2(C, D, E, A, B, WL18, 8, KL1);
		RIP2(B, C, D, E, A, WL19, 13, KL1);
		RIP2(A, B, C, D, E, WL20, 11, KL1);
		RIP2(E, A, B, C, D, WL21, 9, KL1);
		RIP2(D, E, A, B, C, WL22, 7, KL1);
		RIP2(C, D, E, A, B, WL23, 15, KL1);
		RIP2(B, C, D, E, A, WL24, 7, KL1);
		RIP2(A, B, C, D, E, WL25, 12, KL1);
		RIP2(E, A, B, C, D, WL26, 15, KL1);
		RIP2(D, E, A, B, C, WL27, 9, KL1);
		RIP2(C, D, E, A, B, WL28, 11, KL1);
		RIP2(B, C, D, E, A, WL29, 7, KL1);
		RIP2(A, B, C, D, E, WL30, 13, KL1);
		RIP2(E, A, B, C, D, WL31, 12, KL1);

		RIP3(D, E, A, B, C, WL32, 11, KL2);
		RIP3(C, D, E, A, B, WL33, 13, KL2);
		RIP3(B, C, D, E, A, WL34, 6, KL2);
		RIP3(A, B, C, D, E, WL35, 7, KL2);
		RIP3(E, A, B, C, D, WL36, 14, KL2);
		RIP3(D, E, A, B, C, WL37, 9, KL2);
		RIP3(C, D, E, A, B, WL38, 13, KL2);
		RIP3(B, C, D, E, A, WL39, 15, KL2);
		RIP3(A, B, C, D, E, WL40, 14, KL2);
		RIP3(E, A, B, C, D, WL41, 8, KL2);
		RIP3(D, E, A, B, C, WL42, 13, KL2);
		RIP3(C, D, E, A, B, WL43, 6, KL2);
		RIP3(B, C, D, E, A, WL44, 5, KL2);
		RIP3(A, B, C, D, E, WL45, 12, KL2);
		RIP3(E, A, B, C, D, WL46, 7, KL2);
		RIP3(D, E, A, B, C, WL47, 5, KL2);

		RIP4(C, D, E, A, B, WL48, 11, KL3);
		RIP4(B, C, D, E, A, WL49, 12, KL3);
		RIP4(A, B, C, D, E, WL50, 14, KL3);
		RIP4(E, A, B, C, D, WL51, 15, KL3);
		RIP4(D, E, A, B, C, WL52, 14, KL3);
		RIP4(C, D, E, A, B, WL53, 15, KL3);
		RIP4(B, C, D, E, A, WL54, 9, KL3);
		RIP4(A, B, C, D, E, WL55, 8, KL3);
		RIP4(E, A, B, C, D, WL56, 9, KL3);
		RIP4(D, E, A, B, C, WL57, 14, KL3);
		RIP4(C, D, E, A, B, WL58, 5, KL3);
		RIP4(B, C, D, E, A, WL59, 6, KL3);
		RIP4(A, B, C, D, E, WL60, 8, KL3);
		RIP4(E, A, B, C, D, WL61, 6, KL3);
		RIP4(D, E, A, B, C, WL62, 5, KL3);
		RIP4(C, D, E, A, B, WL63, 12, KL3);

		RIP5(B, C, D, E, A, WL64, 9, KL4);
		RIP5(A, B, C, D, E, WL65, 15, KL4);
		RIP5(E, A, B, C, D, WL66, 5, KL4);
		RIP5(D, E, A, B, C, WL67, 11, KL4);
		RIP5(C, D, E, A, B, WL68, 6, KL4);
		RIP5(B, C, D, E, A, WL69, 8, KL4);
		RIP5(A, B, C, D, E, WL70, 13, KL4);
		RIP5(E, A, B, C, D, WL71, 12, KL4);
		RIP5(D, E, A, B, C, WL72, 5, KL4);
		RIP5(C, D, E, A, B, WL73, 12, KL4);
		RIP5(B, C, D, E, A, WL74, 13, KL4);
		RIP5(A, B, C, D, E, WL75, 14, KL4);
		RIP5(E, A, B, C, D, WL76, 11, KL4);
		RIP5(D, E, A, B, C, WL77, 8, KL4);
		RIP5(C, D, E, A, B, WL78, 5, KL4);
		RIP5(B, C, D, E, A, WL79, 6, KL4);

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

		RIP5(A, B, C, D, E, WR00, 8, KR0);
		RIP5(E, A, B, C, D, WR01, 9, KR0);
		RIP5(D, E, A, B, C, WR02, 9, KR0);
		RIP5(C, D, E, A, B, WR03, 11, KR0);
		RIP5(B, C, D, E, A, WR04, 13, KR0);
		RIP5(A, B, C, D, E, WR05, 15, KR0);
		RIP5(E, A, B, C, D, WR06, 15, KR0);
		RIP5(D, E, A, B, C, WR07, 5, KR0);
		RIP5(C, D, E, A, B, WR08, 7, KR0);
		RIP5(B, C, D, E, A, WR09, 7, KR0);
		RIP5(A, B, C, D, E, WR10, 8, KR0);
		RIP5(E, A, B, C, D, WR11, 11, KR0);
		RIP5(D, E, A, B, C, WR12, 14, KR0);
		RIP5(C, D, E, A, B, WR13, 14, KR0);
		RIP5(B, C, D, E, A, WR14, 12, KR0);
		RIP5(A, B, C, D, E, WR15, 6, KR0);

		RIP4(E, A, B, C, D, WR16, 9, KR1);
		RIP4(D, E, A, B, C, WR17, 13, KR1);
		RIP4(C, D, E, A, B, WR18, 15, KR1);
		RIP4(B, C, D, E, A, WR19, 7, KR1);
		RIP4(A, B, C, D, E, WR20, 12, KR1);
		RIP4(E, A, B, C, D, WR21, 8, KR1);
		RIP4(D, E, A, B, C, WR22, 9, KR1);
		RIP4(C, D, E, A, B, WR23, 11, KR1);
		RIP4(B, C, D, E, A, WR24, 7, KR1);
		RIP4(A, B, C, D, E, WR25, 7, KR1);
		RIP4(E, A, B, C, D, WR26, 12, KR1);
		RIP4(D, E, A, B, C, WR27, 7, KR1);
		RIP4(C, D, E, A, B, WR28, 6, KR1);
		RIP4(B, C, D, E, A, WR29, 15, KR1);
		RIP4(A, B, C, D, E, WR30, 13, KR1);
		RIP4(E, A, B, C, D, WR31, 11, KR1);

		RIP3(D, E, A, B, C, WR32, 9, KR2);
		RIP3(C, D, E, A, B, WR33, 7, KR2);
		RIP3(B, C, D, E, A, WR34, 15, KR2);
		RIP3(A, B, C, D, E, WR35, 11, KR2);
		RIP3(E, A, B, C, D, WR36, 8, KR2);
		RIP3(D, E, A, B, C, WR37, 6, KR2);
		RIP3(C, D, E, A, B, WR38, 6, KR2);
		RIP3(B, C, D, E, A, WR39, 14, KR2);
		RIP3(A, B, C, D, E, WR40, 12, KR2);
		RIP3(E, A, B, C, D, WR41, 13, KR2);
		RIP3(D, E, A, B, C, WR42, 5, KR2);
		RIP3(C, D, E, A, B, WR43, 14, KR2);
		RIP3(B, C, D, E, A, WR44, 13, KR2);
		RIP3(A, B, C, D, E, WR45, 13, KR2);
		RIP3(E, A, B, C, D, WR46, 7, KR2);
		RIP3(D, E, A, B, C, WR47, 5, KR2);

		RIP2(C, D, E, A, B, WR48, 15, KR3);
		RIP2(B, C, D, E, A, WR49, 5, KR3);
		RIP2(A, B, C, D, E, WR50, 8, KR3);
		RIP2(E, A, B, C, D, WR51, 11, KR3);
		RIP2(D, E, A, B, C, WR52, 14, KR3);
		RIP2(C, D, E, A, B, WR53, 14, KR3);
		RIP2(B, C, D, E, A, WR54, 6, KR3);
		RIP2(A, B, C, D, E, WR55, 14, KR3);
		RIP2(E, A, B, C, D, WR56, 6, KR3);
		RIP2(D, E, A, B, C, WR57, 9, KR3);
		RIP2(C, D, E, A, B, WR58, 12, KR3);
		RIP2(B, C, D, E, A, WR59, 9, KR3);
		RIP2(A, B, C, D, E, WR60, 12, KR3);
		RIP2(E, A, B, C, D, WR61, 5, KR3);
		RIP2(D, E, A, B, C, WR62, 15, KR3);
		RIP2(C, D, E, A, B, WR63, 8, KR3);

		RIP1(B, C, D, E, A, WR64, 8);
		RIP1(A, B, C, D, E, WR65, 5);
		RIP1(E, A, B, C, D, WR66, 12);
		RIP1(D, E, A, B, C, WR67, 9);
		RIP1(C, D, E, A, B, WR68, 12);
		RIP1(B, C, D, E, A, WR69, 5);
		RIP1(A, B, C, D, E, WR70, 14);
		RIP1(E, A, B, C, D, WR71, 6);
		RIP1(D, E, A, B, C, WR72, 8);
		RIP1(C, D, E, A, B, WR73, 13);
		RIP1(B, C, D, E, A, WR74, 6);
		RIP1(A, B, C, D, E, WR75, 5);
		RIP1(E, A, B, C, D, WR76, 15);
		RIP1(D, E, A, B, C, WR77, 13);
		RIP1(C, D, E, A, B, WR78, 11);
		RIP1(B, C, D, E, A, WR79, 11);

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

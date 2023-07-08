/*	$OpenBSD: sm3.c,v 1.6 2023/07/08 06:36:55 jsing Exp $	*/
/*
 * Copyright (c) 2018, Ribose Inc
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef OPENSSL_NO_SM3

#include <openssl/sm3.h>

#include <string.h>

#include <openssl/opensslconf.h>

#define DATA_ORDER_IS_BIG_ENDIAN

#define HASH_LONG		SM3_WORD
#define HASH_CTX		SM3_CTX
#define HASH_CBLOCK		SM3_CBLOCK
#define HASH_UPDATE		SM3_Update
#define HASH_TRANSFORM		SM3_Transform
#define HASH_FINAL		SM3_Final
#define HASH_MAKE_STRING(c, s) do {		\
	unsigned long ll;			\
	ll = (c)->A; HOST_l2c(ll, (s));		\
	ll = (c)->B; HOST_l2c(ll, (s));		\
	ll = (c)->C; HOST_l2c(ll, (s));		\
	ll = (c)->D; HOST_l2c(ll, (s));		\
	ll = (c)->E; HOST_l2c(ll, (s));		\
	ll = (c)->F; HOST_l2c(ll, (s));		\
	ll = (c)->G; HOST_l2c(ll, (s));		\
	ll = (c)->H; HOST_l2c(ll, (s));		\
} while (0)
#define HASH_BLOCK_DATA_ORDER   SM3_block_data_order

void SM3_block_data_order(SM3_CTX *c, const void *p, size_t num);
void SM3_transform(SM3_CTX *c, const unsigned char *data);

#include "md32_common.h"

#define P0(X) (X ^ ROTATE(X, 9) ^ ROTATE(X, 17))
#define P1(X) (X ^ ROTATE(X, 15) ^ ROTATE(X, 23))

#define FF0(X, Y, Z) (X ^ Y ^ Z)
#define GG0(X, Y, Z) (X ^ Y ^ Z)

#define FF1(X, Y, Z) ((X & Y) | ((X | Y) & Z))
#define GG1(X, Y, Z) ((Z ^ (X & (Y ^ Z))))

#define EXPAND(W0, W7, W13, W3, W10) \
	(P1(W0 ^ W7 ^ ROTATE(W13, 15)) ^ ROTATE(W3, 7) ^ W10)

#define ROUND(A, B, C, D, E, F, G, H, TJ, Wi, Wj, FF, GG)	do {	\
	const SM3_WORD A12 = ROTATE(A, 12);				\
	const SM3_WORD A12_SM = A12 + E + TJ;				\
	const SM3_WORD SS1 = ROTATE(A12_SM, 7);				\
	const SM3_WORD TT1 = FF(A, B, C) + D + (SS1 ^ A12) + (Wj);	\
	const SM3_WORD TT2 = GG(E, F, G) + H + SS1 + Wi;		\
	B = ROTATE(B, 9);						\
	D = TT1;							\
	F = ROTATE(F, 19);						\
	H = P0(TT2);							\
} while(0)

#define R1(A, B, C, D, E, F, G, H, TJ, Wi, Wj) \
	ROUND(A, B, C, D, E, F, G, H, TJ, Wi, Wj, FF0, GG0)

#define R2(A, B, C, D, E, F, G, H, TJ, Wi, Wj) \
	ROUND(A, B, C, D, E, F, G, H, TJ, Wi, Wj, FF1, GG1)

#define SM3_A 0x7380166fUL
#define SM3_B 0x4914b2b9UL
#define SM3_C 0x172442d7UL
#define SM3_D 0xda8a0600UL
#define SM3_E 0xa96f30bcUL
#define SM3_F 0x163138aaUL
#define SM3_G 0xe38dee4dUL
#define SM3_H 0xb0fb0e4eUL

LCRYPTO_ALIAS(SM3_Update);
LCRYPTO_ALIAS(SM3_Final);

int
SM3_Init(SM3_CTX *c)
{
	memset(c, 0, sizeof(*c));
	c->A = SM3_A;
	c->B = SM3_B;
	c->C = SM3_C;
	c->D = SM3_D;
	c->E = SM3_E;
	c->F = SM3_F;
	c->G = SM3_G;
	c->H = SM3_H;
	return 1;
}
LCRYPTO_ALIAS(SM3_Init);

void
SM3_block_data_order(SM3_CTX *ctx, const void *p, size_t num)
{
	const unsigned char *data = p;
	SM3_WORD A, B, C, D, E, F, G, H;
	SM3_WORD W00, W01, W02, W03, W04, W05, W06, W07;
	SM3_WORD W08, W09, W10, W11, W12, W13, W14, W15;

	while (num-- != 0) {
		A = ctx->A;
		B = ctx->B;
		C = ctx->C;
		D = ctx->D;
		E = ctx->E;
		F = ctx->F;
		G = ctx->G;
		H = ctx->H;

        	/*
        	 * We have to load all message bytes immediately since SM3 reads
        	 * them slightly out of order.
        	 */
		HOST_c2l(data, W00);
		HOST_c2l(data, W01);
		HOST_c2l(data, W02);
		HOST_c2l(data, W03);
		HOST_c2l(data, W04);
		HOST_c2l(data, W05);
		HOST_c2l(data, W06);
		HOST_c2l(data, W07);
		HOST_c2l(data, W08);
		HOST_c2l(data, W09);
		HOST_c2l(data, W10);
		HOST_c2l(data, W11);
		HOST_c2l(data, W12);
		HOST_c2l(data, W13);
		HOST_c2l(data, W14);
		HOST_c2l(data, W15);

		R1(A, B, C, D, E, F, G, H, 0x79cc4519, W00, W00 ^ W04);
		W00 = EXPAND(W00, W07, W13, W03, W10);
		R1(D, A, B, C, H, E, F, G, 0xf3988a32, W01, W01 ^ W05);
		W01 = EXPAND(W01, W08, W14, W04, W11);
		R1(C, D, A, B, G, H, E, F, 0xe7311465, W02, W02 ^ W06);
		W02 = EXPAND(W02, W09, W15, W05, W12);
		R1(B, C, D, A, F, G, H, E, 0xce6228cb, W03, W03 ^ W07);
		W03 = EXPAND(W03, W10, W00, W06, W13);
		R1(A, B, C, D, E, F, G, H, 0x9cc45197, W04, W04 ^ W08);
		W04 = EXPAND(W04, W11, W01, W07, W14);
		R1(D, A, B, C, H, E, F, G, 0x3988a32f, W05, W05 ^ W09);
		W05 = EXPAND(W05, W12, W02, W08, W15);
		R1(C, D, A, B, G, H, E, F, 0x7311465e, W06, W06 ^ W10);
		W06 = EXPAND(W06, W13, W03, W09, W00);
		R1(B, C, D, A, F, G, H, E, 0xe6228cbc, W07, W07 ^ W11);
		W07 = EXPAND(W07, W14, W04, W10, W01);
		R1(A, B, C, D, E, F, G, H, 0xcc451979, W08, W08 ^ W12);
		W08 = EXPAND(W08, W15, W05, W11, W02);
		R1(D, A, B, C, H, E, F, G, 0x988a32f3, W09, W09 ^ W13);
		W09 = EXPAND(W09, W00, W06, W12, W03);
		R1(C, D, A, B, G, H, E, F, 0x311465e7, W10, W10 ^ W14);
		W10 = EXPAND(W10, W01, W07, W13, W04);
		R1(B, C, D, A, F, G, H, E, 0x6228cbce, W11, W11 ^ W15);
		W11 = EXPAND(W11, W02, W08, W14, W05);
		R1(A, B, C, D, E, F, G, H, 0xc451979c, W12, W12 ^ W00);
		W12 = EXPAND(W12, W03, W09, W15, W06);
		R1(D, A, B, C, H, E, F, G, 0x88a32f39, W13, W13 ^ W01);
		W13 = EXPAND(W13, W04, W10, W00, W07);
		R1(C, D, A, B, G, H, E, F, 0x11465e73, W14, W14 ^ W02);
		W14 = EXPAND(W14, W05, W11, W01, W08);
		R1(B, C, D, A, F, G, H, E, 0x228cbce6, W15, W15 ^ W03);
		W15 = EXPAND(W15, W06, W12, W02, W09);
		R2(A, B, C, D, E, F, G, H, 0x9d8a7a87, W00, W00 ^ W04);
		W00 = EXPAND(W00, W07, W13, W03, W10);
		R2(D, A, B, C, H, E, F, G, 0x3b14f50f, W01, W01 ^ W05);
		W01 = EXPAND(W01, W08, W14, W04, W11);
		R2(C, D, A, B, G, H, E, F, 0x7629ea1e, W02, W02 ^ W06);
		W02 = EXPAND(W02, W09, W15, W05, W12);
		R2(B, C, D, A, F, G, H, E, 0xec53d43c, W03, W03 ^ W07);
		W03 = EXPAND(W03, W10, W00, W06, W13);
		R2(A, B, C, D, E, F, G, H, 0xd8a7a879, W04, W04 ^ W08);
		W04 = EXPAND(W04, W11, W01, W07, W14);
		R2(D, A, B, C, H, E, F, G, 0xb14f50f3, W05, W05 ^ W09);
		W05 = EXPAND(W05, W12, W02, W08, W15);
		R2(C, D, A, B, G, H, E, F, 0x629ea1e7, W06, W06 ^ W10);
		W06 = EXPAND(W06, W13, W03, W09, W00);
		R2(B, C, D, A, F, G, H, E, 0xc53d43ce, W07, W07 ^ W11);
		W07 = EXPAND(W07, W14, W04, W10, W01);
		R2(A, B, C, D, E, F, G, H, 0x8a7a879d, W08, W08 ^ W12);
		W08 = EXPAND(W08, W15, W05, W11, W02);
		R2(D, A, B, C, H, E, F, G, 0x14f50f3b, W09, W09 ^ W13);
		W09 = EXPAND(W09, W00, W06, W12, W03);
		R2(C, D, A, B, G, H, E, F, 0x29ea1e76, W10, W10 ^ W14);
		W10 = EXPAND(W10, W01, W07, W13, W04);
		R2(B, C, D, A, F, G, H, E, 0x53d43cec, W11, W11 ^ W15);
		W11 = EXPAND(W11, W02, W08, W14, W05);
		R2(A, B, C, D, E, F, G, H, 0xa7a879d8, W12, W12 ^ W00);
		W12 = EXPAND(W12, W03, W09, W15, W06);
		R2(D, A, B, C, H, E, F, G, 0x4f50f3b1, W13, W13 ^ W01);
		W13 = EXPAND(W13, W04, W10, W00, W07);
		R2(C, D, A, B, G, H, E, F, 0x9ea1e762, W14, W14 ^ W02);
		W14 = EXPAND(W14, W05, W11, W01, W08);
		R2(B, C, D, A, F, G, H, E, 0x3d43cec5, W15, W15 ^ W03);
		W15 = EXPAND(W15, W06, W12, W02, W09);
		R2(A, B, C, D, E, F, G, H, 0x7a879d8a, W00, W00 ^ W04);
		W00 = EXPAND(W00, W07, W13, W03, W10);
		R2(D, A, B, C, H, E, F, G, 0xf50f3b14, W01, W01 ^ W05);
		W01 = EXPAND(W01, W08, W14, W04, W11);
		R2(C, D, A, B, G, H, E, F, 0xea1e7629, W02, W02 ^ W06);
		W02 = EXPAND(W02, W09, W15, W05, W12);
		R2(B, C, D, A, F, G, H, E, 0xd43cec53, W03, W03 ^ W07);
		W03 = EXPAND(W03, W10, W00, W06, W13);
		R2(A, B, C, D, E, F, G, H, 0xa879d8a7, W04, W04 ^ W08);
		W04 = EXPAND(W04, W11, W01, W07, W14);
		R2(D, A, B, C, H, E, F, G, 0x50f3b14f, W05, W05 ^ W09);
		W05 = EXPAND(W05, W12, W02, W08, W15);
		R2(C, D, A, B, G, H, E, F, 0xa1e7629e, W06, W06 ^ W10);
		W06 = EXPAND(W06, W13, W03, W09, W00);
		R2(B, C, D, A, F, G, H, E, 0x43cec53d, W07, W07 ^ W11);
		W07 = EXPAND(W07, W14, W04, W10, W01);
		R2(A, B, C, D, E, F, G, H, 0x879d8a7a, W08, W08 ^ W12);
		W08 = EXPAND(W08, W15, W05, W11, W02);
		R2(D, A, B, C, H, E, F, G, 0x0f3b14f5, W09, W09 ^ W13);
		W09 = EXPAND(W09, W00, W06, W12, W03);
		R2(C, D, A, B, G, H, E, F, 0x1e7629ea, W10, W10 ^ W14);
		W10 = EXPAND(W10, W01, W07, W13, W04);
		R2(B, C, D, A, F, G, H, E, 0x3cec53d4, W11, W11 ^ W15);
		W11 = EXPAND(W11, W02, W08, W14, W05);
		R2(A, B, C, D, E, F, G, H, 0x79d8a7a8, W12, W12 ^ W00);
		W12 = EXPAND(W12, W03, W09, W15, W06);
		R2(D, A, B, C, H, E, F, G, 0xf3b14f50, W13, W13 ^ W01);
		W13 = EXPAND(W13, W04, W10, W00, W07);
		R2(C, D, A, B, G, H, E, F, 0xe7629ea1, W14, W14 ^ W02);
		W14 = EXPAND(W14, W05, W11, W01, W08);
		R2(B, C, D, A, F, G, H, E, 0xcec53d43, W15, W15 ^ W03);
		W15 = EXPAND(W15, W06, W12, W02, W09);
		R2(A, B, C, D, E, F, G, H, 0x9d8a7a87, W00, W00 ^ W04);
		W00 = EXPAND(W00, W07, W13, W03, W10);
		R2(D, A, B, C, H, E, F, G, 0x3b14f50f, W01, W01 ^ W05);
		W01 = EXPAND(W01, W08, W14, W04, W11);
		R2(C, D, A, B, G, H, E, F, 0x7629ea1e, W02, W02 ^ W06);
		W02 = EXPAND(W02, W09, W15, W05, W12);
		R2(B, C, D, A, F, G, H, E, 0xec53d43c, W03, W03 ^ W07);
		W03 = EXPAND(W03, W10, W00, W06, W13);
		R2(A, B, C, D, E, F, G, H, 0xd8a7a879, W04, W04 ^ W08);
		R2(D, A, B, C, H, E, F, G, 0xb14f50f3, W05, W05 ^ W09);
		R2(C, D, A, B, G, H, E, F, 0x629ea1e7, W06, W06 ^ W10);
		R2(B, C, D, A, F, G, H, E, 0xc53d43ce, W07, W07 ^ W11);
		R2(A, B, C, D, E, F, G, H, 0x8a7a879d, W08, W08 ^ W12);
		R2(D, A, B, C, H, E, F, G, 0x14f50f3b, W09, W09 ^ W13);
		R2(C, D, A, B, G, H, E, F, 0x29ea1e76, W10, W10 ^ W14);
		R2(B, C, D, A, F, G, H, E, 0x53d43cec, W11, W11 ^ W15);
		R2(A, B, C, D, E, F, G, H, 0xa7a879d8, W12, W12 ^ W00);
		R2(D, A, B, C, H, E, F, G, 0x4f50f3b1, W13, W13 ^ W01);
		R2(C, D, A, B, G, H, E, F, 0x9ea1e762, W14, W14 ^ W02);
		R2(B, C, D, A, F, G, H, E, 0x3d43cec5, W15, W15 ^ W03);

		ctx->A ^= A;
		ctx->B ^= B;
		ctx->C ^= C;
		ctx->D ^= D;
		ctx->E ^= E;
		ctx->F ^= F;
		ctx->G ^= G;
		ctx->H ^= H;
	}
}

#endif /* !OPENSSL_NO_SM3 */

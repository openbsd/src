/*	$OpenBSD: sm3_locl.h,v 1.1 2018/11/11 06:53:31 tb Exp $	*/
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

/* $OpenBSD: gcm128.c,v 1.40 2025/05/18 09:05:59 jsing Exp $ */
/* ====================================================================
 * Copyright (c) 2010 The OpenSSL Project.  All rights reserved.
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
 */

#include <string.h>

#include <openssl/crypto.h>

#include "crypto_internal.h"
#include "modes_local.h"

static void
gcm_init_4bit(u128 Htable[16], uint64_t H[2])
{
	u128 V;
	uint64_t T;
	int i;

	Htable[0].hi = 0;
	Htable[0].lo = 0;
	V.hi = H[0];
	V.lo = H[1];

	for (Htable[8] = V, i = 4; i > 0; i >>= 1) {
		T = U64(0xe100000000000000) & (0 - (V.lo & 1));
		V.lo = (V.hi << 63) | (V.lo >> 1);
		V.hi = (V.hi >> 1 ) ^ T;
		Htable[i] = V;
	}

	for (i = 2; i < 16; i <<= 1) {
		u128 *Hi = Htable + i;
		int   j;
		for (V = *Hi, j = 1; j < i; ++j) {
			Hi[j].hi = V.hi ^ Htable[j].hi;
			Hi[j].lo = V.lo ^ Htable[j].lo;
		}
	}

#if defined(GHASH_ASM) && (defined(__arm__) || defined(__arm))
	/*
	 * ARM assembler expects specific dword order in Htable.
	 */
	{
		int j;
#if BYTE_ORDER == LITTLE_ENDIAN
		for (j = 0; j < 16; ++j) {
			V = Htable[j];
			Htable[j].hi = V.lo;
			Htable[j].lo = V.hi;
		}
#else /* BIG_ENDIAN */
		for (j = 0; j < 16; ++j) {
			V = Htable[j];
			Htable[j].hi = V.lo << 32|V.lo >> 32;
			Htable[j].lo = V.hi << 32|V.hi >> 32;
		}
#endif
	}
#endif
}

#ifndef GHASH_ASM
static const uint16_t rem_4bit[16] = {
	0x0000, 0x1c20, 0x3840, 0x2460, 0x7080, 0x6ca0, 0x48c0, 0x54e0,
	0xe100, 0xfd20, 0xd940, 0xc560, 0x9180, 0x8da0, 0xa9c0, 0xb5e0,
};

static void
gcm_gmult_4bit(uint64_t Xi[2], const u128 Htable[16])
{
	u128 Z;
	int cnt = 15;
	size_t rem, nlo, nhi;

	nlo = ((const uint8_t *)Xi)[15];
	nhi = nlo >> 4;
	nlo &= 0xf;

	Z.hi = Htable[nlo].hi;
	Z.lo = Htable[nlo].lo;

	while (1) {
		rem = (size_t)Z.lo & 0xf;
		Z.lo = (Z.hi << 60)|(Z.lo >> 4);
		Z.hi = (Z.hi >> 4);
		Z.hi ^= (uint64_t)rem_4bit[rem] << 48;
		Z.hi ^= Htable[nhi].hi;
		Z.lo ^= Htable[nhi].lo;

		if (--cnt < 0)
			break;

		nlo = ((const uint8_t *)Xi)[cnt];
		nhi = nlo >> 4;
		nlo &= 0xf;

		rem = (size_t)Z.lo & 0xf;
		Z.lo = (Z.hi << 60)|(Z.lo >> 4);
		Z.hi = (Z.hi >> 4);
		Z.hi ^= (uint64_t)rem_4bit[rem] << 48;
		Z.hi ^= Htable[nlo].hi;
		Z.lo ^= Htable[nlo].lo;
	}

	Xi[0] = htobe64(Z.hi);
	Xi[1] = htobe64(Z.lo);
}

/*
 * Streamed gcm_mult_4bit, see CRYPTO_gcm128_[en|de]crypt for
 * details... Compiler-generated code doesn't seem to give any
 * performance improvement, at least not on x86[_64]. It's here
 * mostly as reference and a placeholder for possible future
 * non-trivial optimization[s]...
 */
static void
gcm_ghash_4bit(uint64_t Xi[2], const u128 Htable[16],
    const uint8_t *inp, size_t len)
{
	u128 Z;
	int cnt;
	size_t rem, nlo, nhi;

#if 1
	do {
		cnt = 15;
		nlo = ((const uint8_t *)Xi)[15];
		nlo ^= inp[15];
		nhi = nlo >> 4;
		nlo &= 0xf;

		Z.hi = Htable[nlo].hi;
		Z.lo = Htable[nlo].lo;

		while (1) {
			rem = (size_t)Z.lo & 0xf;
			Z.lo = (Z.hi << 60)|(Z.lo >> 4);
			Z.hi = (Z.hi >> 4);
			Z.hi ^= (uint64_t)rem_4bit[rem] << 48;
			Z.hi ^= Htable[nhi].hi;
			Z.lo ^= Htable[nhi].lo;

			if (--cnt < 0)
				break;

			nlo = ((const uint8_t *)Xi)[cnt];
			nlo ^= inp[cnt];
			nhi = nlo >> 4;
			nlo &= 0xf;

			rem = (size_t)Z.lo & 0xf;
			Z.lo = (Z.hi << 60)|(Z.lo >> 4);
			Z.hi = (Z.hi >> 4);
			Z.hi ^= (uint64_t)rem_4bit[rem] << 48;
			Z.hi ^= Htable[nlo].hi;
			Z.lo ^= Htable[nlo].lo;
		}
#else
    /*
     * Extra 256+16 bytes per-key plus 512 bytes shared tables
     * [should] give ~50% improvement... One could have PACK()-ed
     * the rem_8bit even here, but the priority is to minimize
     * cache footprint...
     */
	u128 Hshr4[16];	/* Htable shifted right by 4 bits */
	uint8_t Hshl4[16];	/* Htable shifted left  by 4 bits */
	static const unsigned short rem_8bit[256] = {
		0x0000, 0x01C2, 0x0384, 0x0246, 0x0708, 0x06CA, 0x048C, 0x054E,
		0x0E10, 0x0FD2, 0x0D94, 0x0C56, 0x0918, 0x08DA, 0x0A9C, 0x0B5E,
		0x1C20, 0x1DE2, 0x1FA4, 0x1E66, 0x1B28, 0x1AEA, 0x18AC, 0x196E,
		0x1230, 0x13F2, 0x11B4, 0x1076, 0x1538, 0x14FA, 0x16BC, 0x177E,
		0x3840, 0x3982, 0x3BC4, 0x3A06, 0x3F48, 0x3E8A, 0x3CCC, 0x3D0E,
		0x3650, 0x3792, 0x35D4, 0x3416, 0x3158, 0x309A, 0x32DC, 0x331E,
		0x2460, 0x25A2, 0x27E4, 0x2626, 0x2368, 0x22AA, 0x20EC, 0x212E,
		0x2A70, 0x2BB2, 0x29F4, 0x2836, 0x2D78, 0x2CBA, 0x2EFC, 0x2F3E,
		0x7080, 0x7142, 0x7304, 0x72C6, 0x7788, 0x764A, 0x740C, 0x75CE,
		0x7E90, 0x7F52, 0x7D14, 0x7CD6, 0x7998, 0x785A, 0x7A1C, 0x7BDE,
		0x6CA0, 0x6D62, 0x6F24, 0x6EE6, 0x6BA8, 0x6A6A, 0x682C, 0x69EE,
		0x62B0, 0x6372, 0x6134, 0x60F6, 0x65B8, 0x647A, 0x663C, 0x67FE,
		0x48C0, 0x4902, 0x4B44, 0x4A86, 0x4FC8, 0x4E0A, 0x4C4C, 0x4D8E,
		0x46D0, 0x4712, 0x4554, 0x4496, 0x41D8, 0x401A, 0x425C, 0x439E,
		0x54E0, 0x5522, 0x5764, 0x56A6, 0x53E8, 0x522A, 0x506C, 0x51AE,
		0x5AF0, 0x5B32, 0x5974, 0x58B6, 0x5DF8, 0x5C3A, 0x5E7C, 0x5FBE,
		0xE100, 0xE0C2, 0xE284, 0xE346, 0xE608, 0xE7CA, 0xE58C, 0xE44E,
		0xEF10, 0xEED2, 0xEC94, 0xED56, 0xE818, 0xE9DA, 0xEB9C, 0xEA5E,
		0xFD20, 0xFCE2, 0xFEA4, 0xFF66, 0xFA28, 0xFBEA, 0xF9AC, 0xF86E,
		0xF330, 0xF2F2, 0xF0B4, 0xF176, 0xF438, 0xF5FA, 0xF7BC, 0xF67E,
		0xD940, 0xD882, 0xDAC4, 0xDB06, 0xDE48, 0xDF8A, 0xDDCC, 0xDC0E,
		0xD750, 0xD692, 0xD4D4, 0xD516, 0xD058, 0xD19A, 0xD3DC, 0xD21E,
		0xC560, 0xC4A2, 0xC6E4, 0xC726, 0xC268, 0xC3AA, 0xC1EC, 0xC02E,
		0xCB70, 0xCAB2, 0xC8F4, 0xC936, 0xCC78, 0xCDBA, 0xCFFC, 0xCE3E,
		0x9180, 0x9042, 0x9204, 0x93C6, 0x9688, 0x974A, 0x950C, 0x94CE,
		0x9F90, 0x9E52, 0x9C14, 0x9DD6, 0x9898, 0x995A, 0x9B1C, 0x9ADE,
		0x8DA0, 0x8C62, 0x8E24, 0x8FE6, 0x8AA8, 0x8B6A, 0x892C, 0x88EE,
		0x83B0, 0x8272, 0x8034, 0x81F6, 0x84B8, 0x857A, 0x873C, 0x86FE,
		0xA9C0, 0xA802, 0xAA44, 0xAB86, 0xAEC8, 0xAF0A, 0xAD4C, 0xAC8E,
		0xA7D0, 0xA612, 0xA454, 0xA596, 0xA0D8, 0xA11A, 0xA35C, 0xA29E,
		0xB5E0, 0xB422, 0xB664, 0xB7A6, 0xB2E8, 0xB32A, 0xB16C, 0xB0AE,
		0xBBF0, 0xBA32, 0xB874, 0xB9B6, 0xBCF8, 0xBD3A, 0xBF7C, 0xBEBE };
    /*
     * This pre-processing phase slows down procedure by approximately
     * same time as it makes each loop spin faster. In other words
     * single block performance is approximately same as straightforward
     * "4-bit" implementation, and then it goes only faster...
     */
	for (cnt = 0; cnt < 16; ++cnt) {
		Z.hi = Htable[cnt].hi;
		Z.lo = Htable[cnt].lo;
		Hshr4[cnt].lo = (Z.hi << 60)|(Z.lo >> 4);
		Hshr4[cnt].hi = (Z.hi >> 4);
		Hshl4[cnt] = (uint8_t)(Z.lo << 4);
	}

	do {
		for (Z.lo = 0, Z.hi = 0, cnt = 15; cnt; --cnt) {
			nlo = ((const uint8_t *)Xi)[cnt];
			nlo ^= inp[cnt];
			nhi = nlo >> 4;
			nlo &= 0xf;

			Z.hi ^= Htable[nlo].hi;
			Z.lo ^= Htable[nlo].lo;

			rem = (size_t)Z.lo & 0xff;

			Z.lo = (Z.hi << 56)|(Z.lo >> 8);
			Z.hi = (Z.hi >> 8);

			Z.hi ^= Hshr4[nhi].hi;
			Z.lo ^= Hshr4[nhi].lo;
			Z.hi ^= (uint64_t)rem_8bit[rem ^ Hshl4[nhi]] << 48;
		}

		nlo = ((const uint8_t *)Xi)[0];
		nlo ^= inp[0];
		nhi = nlo >> 4;
		nlo &= 0xf;

		Z.hi ^= Htable[nlo].hi;
		Z.lo ^= Htable[nlo].lo;

		rem = (size_t)Z.lo & 0xf;

		Z.lo = (Z.hi << 60)|(Z.lo >> 4);
		Z.hi = (Z.hi >> 4);

		Z.hi ^= Htable[nhi].hi;
		Z.lo ^= Htable[nhi].lo;
		Z.hi ^= ((uint64_t)rem_8bit[rem << 4]) << 48;
#endif

		Xi[0] = htobe64(Z.hi);
		Xi[1] = htobe64(Z.lo);
	} while (inp += 16, len -= 16);
}

static inline void
gcm_mul(GCM128_CONTEXT *ctx, uint64_t u[2])
{
	gcm_gmult_4bit(u, ctx->Htable);
}

static inline void
gcm_ghash(GCM128_CONTEXT *ctx, const uint8_t *in, size_t len)
{
	gcm_ghash_4bit(ctx->Xi.u, ctx->Htable, in, len);
}
#else
void gcm_gmult_4bit(uint64_t Xi[2], const u128 Htable[16]);
void gcm_ghash_4bit(uint64_t Xi[2], const u128 Htable[16], const uint8_t *inp,
    size_t len);

static inline void
gcm_mul(GCM128_CONTEXT *ctx, uint64_t u[2])
{
	ctx->gmult(u, ctx->Htable);
}

static inline void
gcm_ghash(GCM128_CONTEXT *ctx, const uint8_t *in, size_t len)
{
	ctx->ghash(ctx->Xi.u, ctx->Htable, in, len);
}
#endif

/*
 * GHASH_CHUNK is "stride parameter" missioned to mitigate cache
 * trashing effect. In other words idea is to hash data while it's
 * still in L1 cache after encryption pass...
 */
#define GHASH_CHUNK       (3*1024)

#if	defined(GHASH_ASM) &&						\
	(defined(__i386)	|| defined(__i386__)	||		\
	 defined(__x86_64)	|| defined(__x86_64__)	||		\
	 defined(_M_IX86)	|| defined(_M_AMD64)	|| defined(_M_X64))
#include "x86_arch.h"
#endif

#if	defined(GHASH_ASM)
# if	(defined(__i386)	|| defined(__i386__)	||		\
	 defined(__x86_64)	|| defined(__x86_64__)	||		\
	 defined(_M_IX86)	|| defined(_M_AMD64)	|| defined(_M_X64))
#  define GHASH_ASM_X86_OR_64

void gcm_init_clmul(u128 Htable[16], const uint64_t Xi[2]);
void gcm_gmult_clmul(uint64_t Xi[2], const u128 Htable[16]);
void gcm_ghash_clmul(uint64_t Xi[2], const u128 Htable[16], const uint8_t *inp,
    size_t len);

#  if	defined(__i386) || defined(__i386__) || defined(_M_IX86)
#   define GHASH_ASM_X86
void gcm_gmult_4bit_mmx(uint64_t Xi[2], const u128 Htable[16]);
void gcm_ghash_4bit_mmx(uint64_t Xi[2], const u128 Htable[16], const uint8_t *inp,
    size_t len);

void gcm_gmult_4bit_x86(uint64_t Xi[2], const u128 Htable[16]);
void gcm_ghash_4bit_x86(uint64_t Xi[2], const u128 Htable[16], const uint8_t *inp,
    size_t len);
#  endif
# elif defined(__arm__) || defined(__arm)
#  include "arm_arch.h"
#  if __ARM_ARCH__>=7 && !defined(__STRICT_ALIGNMENT)
#   define GHASH_ASM_ARM
void gcm_gmult_neon(uint64_t Xi[2], const u128 Htable[16]);
void gcm_ghash_neon(uint64_t Xi[2], const u128 Htable[16], const uint8_t *inp,
    size_t len);
#  endif
# endif
#endif

void
CRYPTO_gcm128_init(GCM128_CONTEXT *ctx, void *key, block128_f block)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->block = block;
	ctx->key = key;

	(*block)(ctx->H.c, ctx->H.c, key);

	/* H is stored in host byte order */
	ctx->H.u[0] = be64toh(ctx->H.u[0]);
	ctx->H.u[1] = be64toh(ctx->H.u[1]);

# if	defined(GHASH_ASM_X86_OR_64)
#  if	!defined(GHASH_ASM_X86) || defined(OPENSSL_IA32_SSE2)
	/* check FXSR and PCLMULQDQ bits */
	if ((crypto_cpu_caps_ia32() & (CPUCAP_MASK_FXSR | CPUCAP_MASK_PCLMUL)) ==
	    (CPUCAP_MASK_FXSR | CPUCAP_MASK_PCLMUL)) {
		gcm_init_clmul(ctx->Htable, ctx->H.u);
		ctx->gmult = gcm_gmult_clmul;
		ctx->ghash = gcm_ghash_clmul;
		return;
	}
#  endif
	gcm_init_4bit(ctx->Htable, ctx->H.u);
#  if	defined(GHASH_ASM_X86)			/* x86 only */
#   if	defined(OPENSSL_IA32_SSE2)
	if (crypto_cpu_caps_ia32() & CPUCAP_MASK_SSE) {	/* check SSE bit */
#   else
	if (crypto_cpu_caps_ia32() & CPUCAP_MASK_MMX) {	/* check MMX bit */
#   endif
		ctx->gmult = gcm_gmult_4bit_mmx;
		ctx->ghash = gcm_ghash_4bit_mmx;
	} else {
		ctx->gmult = gcm_gmult_4bit_x86;
		ctx->ghash = gcm_ghash_4bit_x86;
	}
#  else
	ctx->gmult = gcm_gmult_4bit;
	ctx->ghash = gcm_ghash_4bit;
#  endif
# elif	defined(GHASH_ASM_ARM)
	if (OPENSSL_armcap_P & ARMV7_NEON) {
		ctx->gmult = gcm_gmult_neon;
		ctx->ghash = gcm_ghash_neon;
	} else {
		gcm_init_4bit(ctx->Htable, ctx->H.u);
		ctx->gmult = gcm_gmult_4bit;
		ctx->ghash = gcm_ghash_4bit;
	}
# else
	gcm_init_4bit(ctx->Htable, ctx->H.u);
# endif
}
LCRYPTO_ALIAS(CRYPTO_gcm128_init);

void
CRYPTO_gcm128_setiv(GCM128_CONTEXT *ctx, const unsigned char *iv, size_t len)
{
	unsigned int ctr;

	ctx->Yi.u[0] = 0;
	ctx->Yi.u[1] = 0;
	ctx->Xi.u[0] = 0;
	ctx->Xi.u[1] = 0;
	ctx->len.u[0] = 0;	/* AAD length */
	ctx->len.u[1] = 0;	/* message length */
	ctx->ares = 0;
	ctx->mres = 0;

	if (len == 12) {
		memcpy(ctx->Yi.c, iv, 12);
		ctx->Yi.c[15] = 1;
		ctr = 1;
	} else {
		size_t i;
		uint64_t len0 = len;

		while (len >= 16) {
			for (i = 0; i < 16; ++i)
				ctx->Yi.c[i] ^= iv[i];
			gcm_mul(ctx, ctx->Yi.u);
			iv += 16;
			len -= 16;
		}
		if (len) {
			for (i = 0; i < len; ++i)
				ctx->Yi.c[i] ^= iv[i];
			gcm_mul(ctx, ctx->Yi.u);
		}
		len0 <<= 3;
		ctx->Yi.u[1] ^= htobe64(len0);

		gcm_mul(ctx, ctx->Yi.u);

		ctr = be32toh(ctx->Yi.d[3]);
	}

	(*ctx->block)(ctx->Yi.c, ctx->EK0.c, ctx->key);
	++ctr;
	ctx->Yi.d[3] = htobe32(ctr);
}
LCRYPTO_ALIAS(CRYPTO_gcm128_setiv);

int
CRYPTO_gcm128_aad(GCM128_CONTEXT *ctx, const unsigned char *aad, size_t len)
{
	size_t i;
	unsigned int n;
	uint64_t alen = ctx->len.u[0];

	if (ctx->len.u[1])
		return -2;

	alen += len;
	if (alen > (U64(1) << 61) || (sizeof(len) == 8 && alen < len))
		return -1;
	ctx->len.u[0] = alen;

	n = ctx->ares;
	if (n) {
		while (n && len) {
			ctx->Xi.c[n] ^= *(aad++);
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0)
			gcm_mul(ctx, ctx->Xi.u);
		else {
			ctx->ares = n;
			return 0;
		}
	}

	if ((i = (len & (size_t)-16))) {
		gcm_ghash(ctx, aad, i);
		aad += i;
		len -= i;
	}
	if (len) {
		n = (unsigned int)len;
		for (i = 0; i < len; ++i)
			ctx->Xi.c[i] ^= aad[i];
	}

	ctx->ares = n;
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_aad);

int
CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx,
    const unsigned char *in, unsigned char *out,
    size_t len)
{
	unsigned int n, ctr;
	size_t i;
	uint64_t mlen = ctx->len.u[1];
	block128_f block = ctx->block;
	void *key = ctx->key;

	mlen += len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	n = ctx->mres;
	if (16 % sizeof(size_t) == 0)
		do {	/* always true actually */
			if (n) {
				while (n && len) {
					ctx->Xi.c[n] ^= *(out++) = *(in++) ^
					    ctx->EKi.c[n];
					--len;
					n = (n + 1) % 16;
				}
				if (n == 0)
					gcm_mul(ctx, ctx->Xi.u);
				else {
					ctx->mres = n;
					return 0;
				}
			}
#ifdef __STRICT_ALIGNMENT
			if (((size_t)in|(size_t)out) % sizeof(size_t) != 0)
				break;
#endif
#if defined(GHASH_CHUNK)
			while (len >= GHASH_CHUNK) {
				size_t j = GHASH_CHUNK;

				while (j) {
					size_t *out_t = (size_t *)out;
					const size_t *in_t = (const size_t *)in;

					(*block)(ctx->Yi.c, ctx->EKi.c, key);
					++ctr;
					ctx->Yi.d[3] = htobe32(ctr);

					for (i = 0; i < 16/sizeof(size_t); ++i)
						out_t[i] = in_t[i] ^
						    ctx->EKi.t[i];
					out += 16;
					in += 16;
					j -= 16;
				}
				gcm_ghash(ctx, out - GHASH_CHUNK, GHASH_CHUNK);
				len -= GHASH_CHUNK;
			}
			if ((i = (len & (size_t)-16))) {
				size_t j = i;

				while (len >= 16) {
					size_t *out_t = (size_t *)out;
					const size_t *in_t = (const size_t *)in;

					(*block)(ctx->Yi.c, ctx->EKi.c, key);
					++ctr;
					ctx->Yi.d[3] = htobe32(ctr);

					for (i = 0; i < 16/sizeof(size_t); ++i)
						out_t[i] = in_t[i] ^
						    ctx->EKi.t[i];
					out += 16;
					in += 16;
					len -= 16;
				}
				gcm_ghash(ctx, out - j, j);
			}
#else
			while (len >= 16) {
				size_t *out_t = (size_t *)out;
				const size_t *in_t = (const size_t *)in;

				(*block)(ctx->Yi.c, ctx->EKi.c, key);
				++ctr;
				ctx->Yi.d[3] = htobe32(ctr);

				for (i = 0; i < 16/sizeof(size_t); ++i)
					ctx->Xi.t[i] ^=
					    out_t[i] = in_t[i] ^ ctx->EKi.t[i];
				gcm_mul(ctx, ctx->Xi.u);
				out += 16;
				in += 16;
				len -= 16;
			}
#endif
			if (len) {
				(*block)(ctx->Yi.c, ctx->EKi.c, key);
				++ctr;
				ctx->Yi.d[3] = htobe32(ctr);

				while (len--) {
					ctx->Xi.c[n] ^= out[n] = in[n] ^
					    ctx->EKi.c[n];
					++n;
				}
			}

			ctx->mres = n;
			return 0;
		} while (0);
	for (i = 0; i < len; ++i) {
		if (n == 0) {
			(*block)(ctx->Yi.c, ctx->EKi.c, key);
			++ctr;
			ctx->Yi.d[3] = htobe32(ctr);
		}
		ctx->Xi.c[n] ^= out[i] = in[i] ^ ctx->EKi.c[n];
		n = (n + 1) % 16;
		if (n == 0)
			gcm_mul(ctx, ctx->Xi.u);
	}

	ctx->mres = n;
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_encrypt);

int
CRYPTO_gcm128_decrypt(GCM128_CONTEXT *ctx,
    const unsigned char *in, unsigned char *out,
    size_t len)
{
	unsigned int n, ctr;
	size_t i;
	uint64_t mlen = ctx->len.u[1];
	block128_f block = ctx->block;
	void *key = ctx->key;

	mlen += len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares) {
		/* First call to decrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	n = ctx->mres;
	if (16 % sizeof(size_t) == 0)
		do {	/* always true actually */
			if (n) {
				while (n && len) {
					uint8_t c = *(in++);
					*(out++) = c ^ ctx->EKi.c[n];
					ctx->Xi.c[n] ^= c;
					--len;
					n = (n + 1) % 16;
				}
				if (n == 0)
					gcm_mul(ctx, ctx->Xi.u);
				else {
					ctx->mres = n;
					return 0;
				}
			}
#ifdef __STRICT_ALIGNMENT
			if (((size_t)in|(size_t)out) % sizeof(size_t) != 0)
				break;
#endif
#if defined(GHASH_CHUNK)
			while (len >= GHASH_CHUNK) {
				size_t j = GHASH_CHUNK;

				gcm_ghash(ctx, in, GHASH_CHUNK);
				while (j) {
					size_t *out_t = (size_t *)out;
					const size_t *in_t = (const size_t *)in;

					(*block)(ctx->Yi.c, ctx->EKi.c, key);
					++ctr;
					ctx->Yi.d[3] = htobe32(ctr);

					for (i = 0; i < 16/sizeof(size_t); ++i)
						out_t[i] = in_t[i] ^
						    ctx->EKi.t[i];
					out += 16;
					in += 16;
					j -= 16;
				}
				len -= GHASH_CHUNK;
			}
			if ((i = (len & (size_t)-16))) {
				gcm_ghash(ctx, in, i);
				while (len >= 16) {
					size_t *out_t = (size_t *)out;
					const size_t *in_t = (const size_t *)in;

					(*block)(ctx->Yi.c, ctx->EKi.c, key);
					++ctr;
					ctx->Yi.d[3] = htobe32(ctr);

					for (i = 0; i < 16/sizeof(size_t); ++i)
						out_t[i] = in_t[i] ^
						    ctx->EKi.t[i];
					out += 16;
					in += 16;
					len -= 16;
				}
			}
#else
			while (len >= 16) {
				size_t *out_t = (size_t *)out;
				const size_t *in_t = (const size_t *)in;

				(*block)(ctx->Yi.c, ctx->EKi.c, key);
				++ctr;
				ctx->Yi.d[3] = htobe32(ctr);

				for (i = 0; i < 16/sizeof(size_t); ++i) {
					size_t c = in_t[i];
					out_t[i] = c ^ ctx->EKi.t[i];
					ctx->Xi.t[i] ^= c;
				}
				gcm_mul(ctx, ctx->Xi.u);
				out += 16;
				in += 16;
				len -= 16;
			}
#endif
			if (len) {
				(*block)(ctx->Yi.c, ctx->EKi.c, key);
				++ctr;
				ctx->Yi.d[3] = htobe32(ctr);

				while (len--) {
					uint8_t c = in[n];
					ctx->Xi.c[n] ^= c;
					out[n] = c ^ ctx->EKi.c[n];
					++n;
				}
			}

			ctx->mres = n;
			return 0;
		} while (0);
	for (i = 0; i < len; ++i) {
		uint8_t c;
		if (n == 0) {
			(*block)(ctx->Yi.c, ctx->EKi.c, key);
			++ctr;
			ctx->Yi.d[3] = htobe32(ctr);
		}
		c = in[i];
		out[i] = c ^ ctx->EKi.c[n];
		ctx->Xi.c[n] ^= c;
		n = (n + 1) % 16;
		if (n == 0)
			gcm_mul(ctx, ctx->Xi.u);
	}

	ctx->mres = n;
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_decrypt);

int
CRYPTO_gcm128_encrypt_ctr32(GCM128_CONTEXT *ctx,
    const unsigned char *in, unsigned char *out,
    size_t len, ctr128_f stream)
{
	unsigned int n, ctr;
	size_t i;
	uint64_t mlen = ctx->len.u[1];
	void *key = ctx->key;

	mlen += len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	n = ctx->mres;
	if (n) {
		while (n && len) {
			ctx->Xi.c[n] ^= *(out++) = *(in++) ^ ctx->EKi.c[n];
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0)
			gcm_mul(ctx, ctx->Xi.u);
		else {
			ctx->mres = n;
			return 0;
		}
	}
#if defined(GHASH_CHUNK)
	while (len >= GHASH_CHUNK) {
		(*stream)(in, out, GHASH_CHUNK/16, key, ctx->Yi.c);
		ctr += GHASH_CHUNK/16;
		ctx->Yi.d[3] = htobe32(ctr);
		gcm_ghash(ctx, out, GHASH_CHUNK);
		out += GHASH_CHUNK;
		in += GHASH_CHUNK;
		len -= GHASH_CHUNK;
	}
#endif
	if ((i = (len & (size_t)-16))) {
		size_t j = i/16;

		(*stream)(in, out, j, key, ctx->Yi.c);
		ctr += (unsigned int)j;
		ctx->Yi.d[3] = htobe32(ctr);
		in += i;
		len -= i;
		gcm_ghash(ctx, out, i);
		out += i;
	}
	if (len) {
		(*ctx->block)(ctx->Yi.c, ctx->EKi.c, key);
		++ctr;
		ctx->Yi.d[3] = htobe32(ctr);
		while (len--) {
			ctx->Xi.c[n] ^= out[n] = in[n] ^ ctx->EKi.c[n];
			++n;
		}
	}

	ctx->mres = n;
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_encrypt_ctr32);

int
CRYPTO_gcm128_decrypt_ctr32(GCM128_CONTEXT *ctx,
    const unsigned char *in, unsigned char *out,
    size_t len, ctr128_f stream)
{
	unsigned int n, ctr;
	size_t i;
	uint64_t mlen = ctx->len.u[1];
	void *key = ctx->key;

	mlen += len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares) {
		/* First call to decrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	n = ctx->mres;
	if (n) {
		while (n && len) {
			uint8_t c = *(in++);
			*(out++) = c ^ ctx->EKi.c[n];
			ctx->Xi.c[n] ^= c;
			--len;
			n = (n + 1) % 16;
		}
		if (n == 0)
			gcm_mul(ctx, ctx->Xi.u);
		else {
			ctx->mres = n;
			return 0;
		}
	}
#if defined(GHASH_CHUNK)
	while (len >= GHASH_CHUNK) {
		gcm_ghash(ctx, in, GHASH_CHUNK);
		(*stream)(in, out, GHASH_CHUNK/16, key, ctx->Yi.c);
		ctr += GHASH_CHUNK/16;
		ctx->Yi.d[3] = htobe32(ctr);
		out += GHASH_CHUNK;
		in += GHASH_CHUNK;
		len -= GHASH_CHUNK;
	}
#endif
	if ((i = (len & (size_t)-16))) {
		size_t j = i/16;

		gcm_ghash(ctx, in, i);
		(*stream)(in, out, j, key, ctx->Yi.c);
		ctr += (unsigned int)j;
		ctx->Yi.d[3] = htobe32(ctr);
		out += i;
		in += i;
		len -= i;
	}
	if (len) {
		(*ctx->block)(ctx->Yi.c, ctx->EKi.c, key);
		++ctr;
		ctx->Yi.d[3] = htobe32(ctr);
		while (len--) {
			uint8_t c = in[n];
			ctx->Xi.c[n] ^= c;
			out[n] = c ^ ctx->EKi.c[n];
			++n;
		}
	}

	ctx->mres = n;
	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_decrypt_ctr32);

int
CRYPTO_gcm128_finish(GCM128_CONTEXT *ctx, const unsigned char *tag,
    size_t len)
{
	uint64_t alen = ctx->len.u[0] << 3;
	uint64_t clen = ctx->len.u[1] << 3;

	if (ctx->mres || ctx->ares)
		gcm_mul(ctx, ctx->Xi.u);

	ctx->Xi.u[0] ^= htobe64(alen);
	ctx->Xi.u[1] ^= htobe64(clen);
	gcm_mul(ctx, ctx->Xi.u);

	ctx->Xi.u[0] ^= ctx->EK0.u[0];
	ctx->Xi.u[1] ^= ctx->EK0.u[1];

	if (tag && len <= sizeof(ctx->Xi))
		return memcmp(ctx->Xi.c, tag, len);
	else
		return -1;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_finish);

void
CRYPTO_gcm128_tag(GCM128_CONTEXT *ctx, unsigned char *tag, size_t len)
{
	CRYPTO_gcm128_finish(ctx, NULL, 0);
	memcpy(tag, ctx->Xi.c,
	    len <= sizeof(ctx->Xi.c) ? len : sizeof(ctx->Xi.c));
}
LCRYPTO_ALIAS(CRYPTO_gcm128_tag);

GCM128_CONTEXT *
CRYPTO_gcm128_new(void *key, block128_f block)
{
	GCM128_CONTEXT *ret;

	if ((ret = malloc(sizeof(GCM128_CONTEXT))))
		CRYPTO_gcm128_init(ret, key, block);

	return ret;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_new);

void
CRYPTO_gcm128_release(GCM128_CONTEXT *ctx)
{
	freezero(ctx, sizeof(*ctx));
}
LCRYPTO_ALIAS(CRYPTO_gcm128_release);

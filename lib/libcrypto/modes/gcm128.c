/* $OpenBSD: gcm128.c,v 1.47 2025/05/22 12:44:14 jsing Exp $ */
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
	ctx->gmult = gcm_gmult_4bit;
	ctx->ghash = gcm_ghash_4bit;
# endif
}
LCRYPTO_ALIAS(CRYPTO_gcm128_init);

GCM128_CONTEXT *
CRYPTO_gcm128_new(void *key, block128_f block)
{
	GCM128_CONTEXT *ctx;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL)
		return NULL;

	CRYPTO_gcm128_init(ctx, key, block);

	return ctx;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_new);

void
CRYPTO_gcm128_release(GCM128_CONTEXT *ctx)
{
	freezero(ctx, sizeof(*ctx));
}
LCRYPTO_ALIAS(CRYPTO_gcm128_release);

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
	unsigned int n;
	uint64_t alen;
	size_t i;

	if (ctx->len.u[1] != 0)
		return -2;

	alen = ctx->len.u[0] + len;
	if (alen > (U64(1) << 61) || (sizeof(len) == 8 && alen < len))
		return -1;
	ctx->len.u[0] = alen;

	if ((n = ctx->ares) > 0) {
		while (n > 0 && len > 0) {
			ctx->Xi.c[n] ^= *(aad++);
			n = (n + 1) % 16;
			len--;
		}
		if (n > 0) {
			ctx->ares = n;
			return 0;
		}
		gcm_mul(ctx, ctx->Xi.u);
	}

	if ((i = (len & (size_t)-16)) > 0) {
		gcm_ghash(ctx, aad, i);
		aad += i;
		len -= i;
	}
	if (len > 0) {
		n = (unsigned int)len;
		for (i = 0; i < len; ++i)
			ctx->Xi.c[i] ^= aad[i];
	}
	ctx->ares = n;

	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_aad);

int
CRYPTO_gcm128_encrypt(GCM128_CONTEXT *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	unsigned int n, ctr;
	uint64_t mlen;
	size_t i;

	mlen = ctx->len.u[1] + len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares > 0) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	n = ctx->mres;

	for (i = 0; i < len; ++i) {
		if (n == 0) {
			ctx->block(ctx->Yi.c, ctx->EKi.c, ctx->key);
			ctx->Yi.d[3] = htobe32(++ctr);
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
CRYPTO_gcm128_decrypt(GCM128_CONTEXT *ctx, const unsigned char *in,
    unsigned char *out, size_t len)
{
	unsigned int n, ctr;
	uint64_t mlen;
	uint8_t c;
	size_t i;

	mlen = ctx->len.u[1] + len;
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

	for (i = 0; i < len; ++i) {
		if (n == 0) {
			ctx->block(ctx->Yi.c, ctx->EKi.c, ctx->key);
			ctx->Yi.d[3] = htobe32(++ctr);
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
CRYPTO_gcm128_encrypt_ctr32(GCM128_CONTEXT *ctx, const unsigned char *in,
    unsigned char *out, size_t len, ctr128_f stream)
{
	unsigned int n, ctr;
	uint64_t mlen;
	size_t i, j;

	mlen = ctx->len.u[1] + len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares > 0) {
		/* First call to encrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	if ((n = ctx->mres) > 0) {
		while (n > 0 && len > 0) {
			ctx->Xi.c[n] ^= *(out++) = *(in++) ^ ctx->EKi.c[n];
			n = (n + 1) % 16;
			len--;
		}
		if (n > 0) {
			ctx->mres = n;
			return 0;
		}
		gcm_mul(ctx, ctx->Xi.u);
	}
	if ((i = (len & (size_t)-16)) > 0) {
		j = i / 16;
		stream(in, out, j, ctx->key, ctx->Yi.c);
		ctr += (unsigned int)j;
		ctx->Yi.d[3] = htobe32(ctr);
		gcm_ghash(ctx, out, i);
		in += i;
		out += i;
		len -= i;
	}
	if (len > 0) {
		ctx->block(ctx->Yi.c, ctx->EKi.c, ctx->key);
		ctx->Yi.d[3] = htobe32(++ctr);
		while (len-- > 0) {
			ctx->Xi.c[n] ^= out[n] = in[n] ^ ctx->EKi.c[n];
			n++;
		}
	}

	ctx->mres = n;

	return 0;
}
LCRYPTO_ALIAS(CRYPTO_gcm128_encrypt_ctr32);

int
CRYPTO_gcm128_decrypt_ctr32(GCM128_CONTEXT *ctx, const unsigned char *in,
    unsigned char *out, size_t len, ctr128_f stream)
{
	unsigned int n, ctr;
	uint64_t mlen;
	size_t i, j;
	uint8_t c;

	mlen = ctx->len.u[1] + len;
	if (mlen > ((U64(1) << 36) - 32) || (sizeof(len) == 8 && mlen < len))
		return -1;
	ctx->len.u[1] = mlen;

	if (ctx->ares > 0) {
		/* First call to decrypt finalizes GHASH(AAD) */
		gcm_mul(ctx, ctx->Xi.u);
		ctx->ares = 0;
	}

	ctr = be32toh(ctx->Yi.d[3]);

	if ((n = ctx->mres) > 0) {
		while (n > 0 && len > 0) {
			c = *(in++);
			*(out++) = c ^ ctx->EKi.c[n];
			ctx->Xi.c[n] ^= c;
			n = (n + 1) % 16;
			len--;
		}
		if (n > 0) {
			ctx->mres = n;
			return 0;
		}
		gcm_mul(ctx, ctx->Xi.u);
	}
	if ((i = (len & (size_t)-16)) > 0) {
		j = i / 16;
		gcm_ghash(ctx, in, i);
		stream(in, out, j, ctx->key, ctx->Yi.c);
		ctr += (unsigned int)j;
		ctx->Yi.d[3] = htobe32(ctr);
		in += i;
		out += i;
		len -= i;
	}
	if (len > 0) {
		ctx->block(ctx->Yi.c, ctx->EKi.c, ctx->key);
		ctx->Yi.d[3] = htobe32(++ctr);
		while (len-- > 0) {
			c = in[n];
			ctx->Xi.c[n] ^= c;
			out[n] = c ^ ctx->EKi.c[n];
			n++;
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
	uint64_t alen, clen;

	alen = ctx->len.u[0] << 3;
	clen = ctx->len.u[1] << 3;

	if (ctx->ares > 0 || ctx->mres > 0)
		gcm_mul(ctx, ctx->Xi.u);

	ctx->Xi.u[0] ^= htobe64(alen);
	ctx->Xi.u[1] ^= htobe64(clen);
	gcm_mul(ctx, ctx->Xi.u);

	ctx->Xi.u[0] ^= ctx->EK0.u[0];
	ctx->Xi.u[1] ^= ctx->EK0.u[1];

	if (tag == NULL || len > sizeof(ctx->Xi))
		return -1;

	return timingsafe_memcmp(ctx->Xi.c, tag, len);
}
LCRYPTO_ALIAS(CRYPTO_gcm128_finish);

void
CRYPTO_gcm128_tag(GCM128_CONTEXT *ctx, unsigned char *tag, size_t len)
{
	CRYPTO_gcm128_finish(ctx, NULL, 0);

	if (len > sizeof(ctx->Xi.c))
		len = sizeof(ctx->Xi.c);

	memcpy(tag, ctx->Xi.c, len);
}
LCRYPTO_ALIAS(CRYPTO_gcm128_tag);

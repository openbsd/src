/*	$OpenBSD: bn_internal.h,v 1.2 2023/02/14 17:58:26 jsing Exp $ */
/*
 * Copyright (c) 2023 Joel Sing <jsing@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

#include <openssl/bn.h>

#include "bn_arch.h"

#ifndef HEADER_BN_INTERNAL_H
#define HEADER_BN_INTERNAL_H

#ifndef HAVE_BN_CT_NE_ZERO
static inline int
bn_ct_ne_zero(BN_ULONG w)
{
	return (w | ~(w - 1)) >> (BN_BITS2 - 1);
}
#endif

#ifndef HAVE_BN_CT_NE_ZERO_MASK
static inline BN_ULONG
bn_ct_ne_zero_mask(BN_ULONG w)
{
	return 0 - bn_ct_ne_zero(w);
}
#endif

#ifndef HAVE_BN_CT_EQ_ZERO
static inline int
bn_ct_eq_zero(BN_ULONG w)
{
	return 1 - bn_ct_ne_zero(w);
}
#endif

#ifndef HAVE_BN_CT_EQ_ZERO_MASK
static inline BN_ULONG
bn_ct_eq_zero_mask(BN_ULONG w)
{
	return 0 - bn_ct_eq_zero(w);
}
#endif

#ifndef HAVE_BN_UMUL_HILO
#ifdef BN_LLONG
static inline void
bn_umul_hilo(BN_ULONG a, BN_ULONG b, BN_ULONG *out_h, BN_ULONG *out_l)
{
	BN_ULLONG r;

	r = (BN_ULLONG)a * (BN_ULLONG)b;

	*out_h = r >> BN_BITS2;
	*out_l = r & BN_MASK2;
}

#else /* !BN_LLONG */
/*
 * Multiply two words (a * b) producing a double word result (h:l).
 *
 * This can be rewritten as:
 *
 *  a * b = (hi32(a) * 2^32 + lo32(a)) * (hi32(b) * 2^32 + lo32(b))
 *        = hi32(a) * hi32(b) * 2^64 +
 *          hi32(a) * lo32(b) * 2^32 +
 *          hi32(b) * lo32(a) * 2^32 +
 *          lo32(a) * lo32(b)
 *
 * The multiplication for each part of a and b can be calculated for each of
 * these four terms without overflowing a BN_ULONG, as the maximum value of a
 * 32 bit x 32 bit multiplication is 32 + 32 = 64 bits. Once these
 * multiplications have been performed the result can be partitioned and summed
 * into a double word (h:l). The same applies on a 32 bit system, substituting
 * 16 for 32 and 32 for 64.
 */
#if 1
static inline void
bn_umul_hilo(BN_ULONG a, BN_ULONG b, BN_ULONG *out_h, BN_ULONG *out_l)
{
	BN_ULONG ah, al, bh, bl, h, l, x, c1, c2;

	ah = a >> BN_BITS4;
	al = a & BN_MASK2l;
	bh = b >> BN_BITS4;
	bl = b & BN_MASK2l;

	h = ah * bh;
	l = al * bl;

	/* (ah * bl) << BN_BITS4, partition the result across h:l with carry. */
	x = ah * bl;
	h += x >> BN_BITS4;
	x <<= BN_BITS4;
	c1 = l | x;
	c2 = l & x;
	l += x;
	h += ((c1 & ~l) | c2) >> (BN_BITS2 - 1); /* carry */
	
	/* (bh * al) << BN_BITS4, partition the result across h:l with carry. */
	x = bh * al;
	h += x >> BN_BITS4;
	x <<= BN_BITS4;
	c1 = l | x;
	c2 = l & x;
	l += x;
	h += ((c1 & ~l) | c2) >> (BN_BITS2 - 1); /* carry */

	*out_h = h;
	*out_l = l;
}
#else

/*
 * XXX - this accumulator based version uses fewer instructions, however
 * requires more variables/registers. It seems to be slower on at least amd64
 * and i386, however may be faster on other architectures that have more
 * registers available. Further testing is required and one of the two
 * implementations should eventually be removed.
 */
static inline void
bn_umul_hilo(BN_ULONG a, BN_ULONG b, BN_ULONG *out_h, BN_ULONG *out_l)
{
	BN_ULONG ah, bh, al, bl, x, h, l;
	BN_ULONG acc0, acc1, acc2, acc3;

	ah = a >> BN_BITS4;
	bh = b >> BN_BITS4;
	al = a & BN_MASK2l;
	bl = b & BN_MASK2l;

	h = ah * bh;
	l = al * bl;

	acc0 = l & BN_MASK2l;
	acc1 = l >> BN_BITS4;
	acc2 = h & BN_MASK2l;
	acc3 = h >> BN_BITS4;

	/* (ah * bl) << BN_BITS4, partition the result across h:l. */
	x = ah * bl;
	acc1 += x & BN_MASK2l;
	acc2 += (acc1 >> BN_BITS4) + (x >> BN_BITS4);
	acc3 += acc2 >> BN_BITS4;

	/* (bh * al) << BN_BITS4, partition the result across h:l. */
	x = bh * al;
	acc1 += x & BN_MASK2l;
	acc2 += (acc1 >> BN_BITS4) + (x >> BN_BITS4);
	acc3 += acc2 >> BN_BITS4;

	*out_h = (acc3 << BN_BITS4) | acc2;
	*out_l = (acc1 << BN_BITS4) | acc0;
}
#endif
#endif /* !BN_LLONG */
#endif

#ifndef HAVE_BN_UMUL_LO
static inline BN_ULONG
bn_umul_lo(BN_ULONG a, BN_ULONG b)
{
	return a * b;
}
#endif

#ifndef HAVE_BN_UMUL_HI
static inline BN_ULONG
bn_umul_hi(BN_ULONG a, BN_ULONG b)
{
	BN_ULONG h, l;

	bn_umul_hilo(a, b, &h, &l);

	return h;
}
#endif

#endif

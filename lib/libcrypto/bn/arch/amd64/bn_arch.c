/*	$OpenBSD: bn_arch.c,v 1.7 2023/06/24 16:01:44 jsing Exp $ */
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
#include "bn_local.h"
#include "s2n_bignum.h"

#ifdef HAVE_BN_ADD
BN_ULONG
bn_add(BN_ULONG *r, int r_len, const BN_ULONG *a, int a_len, const BN_ULONG *b,
    int b_len)
{
	return bignum_add(r_len, (uint64_t *)r, a_len, (uint64_t *)a,
	    b_len, (uint64_t *)b);
}
#endif


#ifdef HAVE_BN_ADD_WORDS
BN_ULONG
bn_add_words(BN_ULONG *rd, const BN_ULONG *ad, const BN_ULONG *bd, int n)
{
	return bignum_add(n, (uint64_t *)rd, n, (uint64_t *)ad, n,
	    (uint64_t *)bd);
}
#endif

#ifdef HAVE_BN_SUB
BN_ULONG
bn_sub(BN_ULONG *r, int r_len, const BN_ULONG *a, int a_len, const BN_ULONG *b,
    int b_len)
{
	return bignum_sub(r_len, (uint64_t *)r, a_len, (uint64_t *)a,
	    b_len, (uint64_t *)b);
}
#endif

#ifdef HAVE_BN_SUB_WORDS
BN_ULONG
bn_sub_words(BN_ULONG *rd, const BN_ULONG *ad, const BN_ULONG *bd, int n)
{
	return bignum_sub(n, (uint64_t *)rd, n, (uint64_t *)ad, n,
	    (uint64_t *)bd);
}
#endif

#ifdef HAVE_BN_MUL_ADD_WORDS
BN_ULONG
bn_mul_add_words(BN_ULONG *rd, const BN_ULONG *ad, int num, BN_ULONG w)
{
	return bignum_cmadd(num, (uint64_t *)rd, w, num, (uint64_t *)ad);
}
#endif

#ifdef HAVE_BN_MUL_WORDS
BN_ULONG
bn_mul_words(BN_ULONG *rd, const BN_ULONG *ad, int num, BN_ULONG w)
{
	return bignum_cmul(num, (uint64_t *)rd, w, num, (uint64_t *)ad);
}
#endif

#ifdef HAVE_BN_MUL_COMBA4
void
bn_mul_comba4(BN_ULONG *rd, BN_ULONG *ad, BN_ULONG *bd)
{
	/* XXX - consider using non-alt on CPUs that have the ADX extension. */
	bignum_mul_4_8_alt((uint64_t *)rd, (uint64_t *)ad, (uint64_t *)bd);
}
#endif

#ifdef HAVE_BN_MUL_COMBA8
void
bn_mul_comba8(BN_ULONG *rd, BN_ULONG *ad, BN_ULONG *bd)
{
	/* XXX - consider using non-alt on CPUs that have the ADX extension. */
	bignum_mul_8_16_alt((uint64_t *)rd, (uint64_t *)ad, (uint64_t *)bd);
}
#endif

#ifdef HAVE_BN_SQR
int
bn_sqr(BIGNUM *r, const BIGNUM *a, int r_len, BN_CTX *ctx)
{
	bignum_sqr(r_len, (uint64_t *)r->d, a->top, (uint64_t *)a->d);

	return 1;
}
#endif

#ifdef HAVE_BN_SQR_COMBA4
void
bn_sqr_comba4(BN_ULONG *rd, const BN_ULONG *ad)
{
	/* XXX - consider using non-alt on CPUs that have the ADX extension. */
	bignum_sqr_4_8_alt((uint64_t *)rd, (uint64_t *)ad);
}
#endif

#ifdef HAVE_BN_SQR_COMBA8
void
bn_sqr_comba8(BN_ULONG *rd, const BN_ULONG *ad)
{
	/* XXX - consider using non-alt on CPUs that have the ADX extension. */
	bignum_sqr_8_16_alt((uint64_t *)rd, (uint64_t *)ad);
}
#endif

#ifdef HAVE_BN_WORD_CLZ
int
bn_word_clz(BN_ULONG w)
{
	return word_clz(w);
}
#endif

/*	$OpenBSD: bn_mod_words.c,v 1.1 2025/05/25 04:58:32 jsing Exp $	*/
/*
 * Copyright (c) 2024 Joel Sing <jsing@openbsd.org>
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

#include "bn_local.h"
#include "bn_internal.h"

/*
 * bn_mod_add_words() computes r[] = (a[] + b[]) mod m[], where a, b, r and
 * m are arrays of words with length n (r may be the same as a or b).
 */
#ifndef HAVE_BN_MOD_ADD_WORDS
void
bn_mod_add_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
    const BN_ULONG *m, size_t n)
{
	BN_ULONG carry, mask;

	/*
	 * Compute a + b, then compute r - m to determine if r >= m, considering
	 * any carry that resulted from the addition. Finally complete a
	 * conditional subtraction of r - m.
	 */
	/* XXX - change bn_add_words to use size_t. */
	carry = bn_add_words(r, a, b, n);
	mask = ~(carry - bn_sub_words_borrow(r, m, n));
	bn_sub_words_masked(r, r, m, mask, n);
}
#endif

/*
 * bn_mod_sub_words() computes r[] = (a[] - b[]) mod m[], where a, b, r and
 * m are arrays of words with length n (r may be the same as a or b).
 */
#ifndef HAVE_BN_MOD_SUB_WORDS
void
bn_mod_sub_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
    const BN_ULONG *m, size_t n)
{
	BN_ULONG borrow, mask;

	/*
	 * Compute a - b, then complete a conditional addition of r + m
	 * based on the resulting borrow.
	 */
	/* XXX - change bn_sub_words to use size_t. */
	borrow = bn_sub_words(r, a, b, n);
	mask = (0 - borrow);
	bn_add_words_masked(r, r, m, mask, n);
}
#endif

/*
 * bn_mod_mul_words() computes r[] = (a[] * b[]) mod m[], where a, b, r and
 * m are arrays of words with length n (r may be the same as a or b) in the
 * Montgomery domain. The result remains in the Montgomery domain.
 */
#ifndef HAVE_BN_MOD_MUL_WORDS
void
bn_mod_mul_words(BN_ULONG *r, const BN_ULONG *a, const BN_ULONG *b,
    const BN_ULONG *m, BN_ULONG *t, BN_ULONG m0, size_t n)
{
	bn_montgomery_multiply_words(r, a, b, m, t, m0, n);
}
#endif

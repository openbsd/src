/*	$OpenBSD: fpu_compare.c,v 1.3 2007/12/26 18:27:43 miod Exp $	*/

/*
 * Copyright (c) 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fpu_compare.c	8.1 (Berkeley) 6/11/93
 */

/*
 * fcmp and fcmpu instructions.
 *
 * These rely on the fact that our internal wide format is achieved by
 * adding zero bits to the end of narrower mantissas.
 */

#include <sys/types.h>

#include <machine/fpu.h>
#include <machine/frame.h>

#include <m88k/fpu/fpu_arith.h>
#include <m88k/fpu/fpu_emu.h>

/*
 * Perform a compare instruction.
 *
 * If either operand is NaN, the result is unordered.  This causes an
 * reserved operand exception (except for nonsignalling NaNs for fcmpu).
 *
 * Everything else is ordered:
 *	|Inf| > |numbers| > |0|.
 * We already arranged for fp_class(Inf) > fp_class(numbers) > fp_class(0),
 * so we get this directly.  Note, however, that two zeros compare equal
 * regardless of sign, while everything else depends on sign.
 *
 * Incidentally, two Infs of the same sign compare equal. Since the 88110 
 * does infinity arithmetic on hardware, this codepath should never be
 * entered.
 */
u_int32_t
fpu_compare(struct fpemu *fe, int fcmpu)
{
	struct fpn *a, *b;
	u_int32_t cc;
	int r3, r2, r1, r0;
	FPU_DECL_CARRY

	a = &fe->fe_f1;
	b = &fe->fe_f2;

	/* fcmpu shall only raise an exception for signalling NaNs */
	if (ISNAN(a)) {
		if (!fcmpu || (a->fp_mant[0] & FP_QUIETBIT) != 0)
			fe->fe_fpsr |= FPSR_EFINV;
		cc = CC_UN;
		goto done;
	}
	if (ISNAN(b)) {
		if (!fcmpu || (b->fp_mant[0] & FP_QUIETBIT) != 0)
			fe->fe_fpsr |= FPSR_EFINV;
		cc = CC_UN;
		goto done;
	}

	/*
	 * Must handle both-zero early to avoid sign goofs.  Otherwise,
	 * at most one is 0, and if the signs differ we are done.
	 */
	if (ISZERO(a) && ISZERO(b)) {
		cc = CC_EQ;
		goto done;
	}
	if (a->fp_sign) {		/* a < 0 (or -0) */
		if (!b->fp_sign) {	/* b >= 0 (or if a = -0, b > 0) */
			cc = CC_LT;
			goto done;
		}
	} else {			/* a > 0 (or +0) */
		if (b->fp_sign) {	/* b <= -0 (or if a = +0, b < 0) */
			cc = CC_GT;
			goto done;
		}
	}

	/*
	 * Now the signs are the same (but may both be negative).  All
	 * we have left are these cases:
	 *
	 *	|a| < |b|		[classes or values differ]
	 *	|a| > |b|		[classes or values differ]
	 *	|a| == |b|		[classes and values identical]
	 *
	 * We define `diff' here to expand these as:
	 *
	 *	|a| < |b|, a,b >= 0: a < b => CC_LT
	 *	|a| < |b|, a,b < 0:  a > b => CC_GT
	 *	|a| > |b|, a,b >= 0: a > b => CC_GT
	 *	|a| > |b|, a,b < 0:  a < b => CC_LT
	 */
#define opposite_cc(cc) ((cc) == CC_LT ? CC_GT : CC_LT)
#define	diff(magnitude) (a->fp_sign ? opposite_cc(magnitude) :  (magnitude))
	if (a->fp_class < b->fp_class) {	/* |a| < |b| */
		cc = diff(CC_LT);
		goto done;
	}
	if (a->fp_class > b->fp_class) {	/* |a| > |b| */
		cc = diff(CC_GT);
		goto done;
	}
	/* now none can be 0: only Inf and numbers remain */
	if (ISINF(a)) {				/* |Inf| = |Inf| */
		cc = CC_EQ;
		goto done;
	}
	/*
	 * Only numbers remain.  To compare two numbers in magnitude, we
	 * simply subtract their mantissas.
	 */
	FPU_SUBS(r3, a->fp_mant[0], b->fp_mant[0]);
	FPU_SUBCS(r2, a->fp_mant[1], b->fp_mant[1]);
	FPU_SUBCS(r1, a->fp_mant[2], b->fp_mant[2]);
	FPU_SUBC(r0, a->fp_mant[3], b->fp_mant[3]);
	if (r0 < 0)				/* underflow: |a| < |b| */
		cc = diff(CC_LT);
	else if ((r0 | r1 | r2 | r3) != 0)	/* |a| > |b| */
		cc = diff(CC_GT);
	else
		cc = CC_EQ;		/* |a| == |b| */
done:

	/*
	 * Complete condition code mask.
	 */

	if (cc & CC_UN)
		cc |= CC_UE | CC_UG | CC_ULE | CC_UL | CC_UGE;
	if (cc & CC_EQ)
		cc |= CC_LE | CC_GE | CC_UE;
	if (cc & CC_GT)
		cc |= CC_GE;
	if (cc & CC_LT)
		cc |= CC_LE;
	if (cc & (CC_LT | CC_GT))
		cc |= CC_LG;
	if (cc & (CC_LT | CC_GT | CC_EQ))
		cc |= CC_LEG;
	if (cc & CC_GT)
		cc |= CC_UG;
	if (cc & CC_LE)
		cc |= CC_ULE;
	if (cc & CC_LT)
		cc |= CC_UL;
	if (cc & CC_GE)
		cc |= CC_UGE;

	/*
	 * Fill the interval bits.
	 * s1 (here `a') is compared to the interval [0, s2 (here `b')].
	 */
	if (!(cc & CC_UN)) {
		/* s1 and s2 are either Zero, numbers or Inf */
		if (ISZERO(a) || (cc & CC_EQ)) {
			/* if s1 and s2 are equal, s1 is on boundary */
			cc |= CC_IB | CC_OB;
		} else if (b->fp_sign == 0) {
			/* s2 is positive, the interval is [0, s2] */
			if (cc & CC_GT) {
				/* 0 <= s2 < s1 -> out of interval */
				cc |= CC_OU | CC_OB;
			} else if (a->fp_sign == 0) {
				/* 0 < s1 < s2 -> in interval */
				cc |= CC_IB | CC_IN;
			} else {
				/* s1 < 0 <= s2 */
				cc |= CC_OU | CC_OB;
			}
		} else {
			/* s2 is negative, the interval is [s2, 0] */
			if (cc & CC_LT) {
				/* s1 < s2 <= 0 */
				cc |= CC_OU | CC_OB;
			} else if (a->fp_sign != 0) {
				/* s2 < s1 < 0 */
				cc |= CC_IB | CC_IN;
			} else {
				/* s2 < 0 < s1 */
				cc |= CC_OU | CC_OB;
			}
		}
	}

	return (cc);
}

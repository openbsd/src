/*	$OpenBSD: ldexp.c,v 1.3 2008/06/26 05:42:04 ray Exp $	*/
/*	$NetBSD: ldexp.c,v 1.2 2001/11/08 22:45:45 bjh21 Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <machine/ieee.h>
#include <errno.h>
#include <math.h>

/*
 * Multiply the given value by 2^exponent.
 */
double
ldexp(val, expo)
	double val;
	int expo;
{
	register int oldexp, newexp;
	union {
		double v;
		struct ieee_double s;
	} u, mul;

	u.v = val;
	oldexp = u.s.dbl_exp;

	/*
	 * If input is zero, Inf or NaN, just return it.
	 */
	if (u.v == 0.0 || oldexp == DBL_EXP_INFNAN)
		return (val);

	if (oldexp == 0) {
		/*
		 * u.v is denormal.  We must adjust it so that the exponent
		 * arithmetic below will work.
		 */
		if (expo <= DBL_EXP_BIAS) {
			/*
			 * Optimization: if the scaling can be done in a single
			 * multiply, or underflows, just do it now.
			 */
			if (expo <= -DBL_FRACBITS) {
				errno = ERANGE;
				return (0.0);
			}
			mul.v = 0.0;
			mul.s.dbl_exp = expo + DBL_EXP_BIAS;
			u.v *= mul.v;
			if (u.v == 0.0) {
				errno = ERANGE;
				return (0.0);
			}
			return (u.v);
		} else {
			/*
			 * We know that expo is very large, and therefore the
			 * result cannot be denormal (though it may be Inf).
			 * Shift u.v by just enough to make it normal.
			 */
			mul.v = 0.0;
			mul.s.dbl_exp = DBL_FRACBITS + DBL_EXP_BIAS;
			u.v *= mul.v;
			expo -= DBL_FRACBITS;
			oldexp = u.s.dbl_exp;
		}
	}

	/*
	 * u.v is now normalized and oldexp has been adjusted if necessary.
	 * Calculate the new exponent and check for underflow and overflow.
	 */
	newexp = oldexp + expo;

	if (newexp <= 0) {
		/*
		 * The output number is either denormal or underflows (see
		 * comments in machine/ieee.h).
		 */
		if (newexp <= -DBL_FRACBITS) {
			errno = ERANGE;
			return (0.0);
		}
		/*
		 * Denormalize the result.  We do this with a multiply. If expo
		 * is very large, it won't fit in a double, so we have to
		 * adjust the exponent first.  This is safe because we know
		 * that u.v is normal at this point.
		 */
		if (expo <= -DBL_EXP_BIAS) {
			u.s.dbl_exp = 1;
			expo += oldexp - 1;
		}
		mul.v = 0.0;
		mul.s.dbl_exp = expo + DBL_EXP_BIAS;
		u.v *= mul.v;
		return (u.v);
	} else if (newexp >= DBL_EXP_INFNAN) {
		/*
		 * The result overflowed; return +/-Inf.
		 */
		u.s.dbl_exp = DBL_EXP_INFNAN;
		u.s.dbl_frach = 0;
		u.s.dbl_fracl = 0;
		errno = ERANGE;
		return (u.v);
	} else {
		/*
		 * The result is normal; just replace the old exponent with the
		 * new one.
		 */
		u.s.dbl_exp = newexp;
		return (u.v);
	}
}

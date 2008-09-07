/*	$OpenBSD: s_fmax.c,v 1.1 2008/09/07 20:36:09 martynas Exp $	*/
/*-
 * Copyright (c) 2004 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include <sys/types.h>
#include <machine/ieee.h>
#include <math.h>

double
fmax(double x, double y)
{
	struct ieee_double *px = (struct ieee_double *)&x;
	struct ieee_double *py = (struct ieee_double *)&y;

	/* Check for NaNs to avoid raising spurious exceptions. */
	if (px->dbl_exp == DBL_EXP_INFNAN &&
	    (px->dbl_frach | px->dbl_fracl) != 0)
		return (y);
	if (py->dbl_exp == DBL_EXP_INFNAN &&
	    (py->dbl_frach | py->dbl_fracl) != 0)
		return (x);

	/* Handle comparisons of signed zeroes. */
	if (px->dbl_sign != py->dbl_sign &&
	    px->dbl_sign == 1)
		return (y);
	if (px->dbl_sign != py->dbl_sign &&
	    px->dbl_sign == 0)
		return (x);

	return (x > y ? x : y);
}

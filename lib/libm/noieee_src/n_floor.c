/*	$OpenBSD: n_floor.c,v 1.13 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_floor.c,v 1.1 1995/10/10 23:36:48 ragge Exp $	*/
/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
#include <math.h>

#include "mathimpl.h"

static const double L = 36028797018963968.0E0;

/*
 * floor(x) := the largest integer no larger than x;
 * ceil(x) := -floor(-x), for all real x.
 *
 * Note: Inexact will be signaled if x is not an integer, as is
 *	customary for IEEE 754.  No other signal can be emitted.
 */
double
floor(double x)
{
	volatile double y;

	if (isnan(x) ||	x >= L)		/* already an even integer */
		return x;
	else if (x < (double)0)
		return -ceil(-x);
	else {			/* now 0 <= x < L */
		y = L+x;		/* destructive store must be forced */
		y -= L;			/* an integer, and |x-y| < 1 */
		return x < y ? y-(double)1 : y;
	}
}

double
ceil(double x)
{
	volatile double y;

	if (isnan(x) ||	x >= L)		/* already an even integer */
		return x;
	else if (x < (double)0)
		return -floor(-x);
	else {			/* now 0 <= x < L */
		y = L+x;		/* destructive store must be forced */
		y -= L;			/* an integer, and |x-y| < 1 */
		return x > y ? y+(double)1 : y;
	}
}

/*
 * algorithm for rint(x) in pseudo-pascal form ...
 *
 * real rint(x): real x;
 *	... delivers integer nearest x in direction of prevailing rounding
 *	... mode
 * const	L = (last consecutive integer)/2
 * 	  = 2**55; for VAX D
 * 	  = 2**52; for IEEE 754 Double
 * real	s,t;
 * begin
 * 	if isnan(x) then return x;		... NaN
 * 	if |x| >= L then return x;		... already an integer
 * 	s := copysign(L,x);
 * 	t := x + s;				... = (x+s) rounded to integer
 * 	return t - s
 * end;
 *
 * Note: Inexact will be signaled if x is not an integer, as is
 *	customary for IEEE 754.  No other signal can be emitted.
 */
double
rint(double x)
{
	double s;
	volatile double t;
	const double one = 1.0;

	if (isnan(x))
		return (x);

	if (copysign(x, one) >= L)	/* already an integer */
		return (x);

	s = copysign(L,x);
	t = x + s;				/* x+s rounded to integer */
	return (t - s);
}

#ifdef __weak_alias    
__weak_alias(rintl, rint);
#endif /* __weak_alias */

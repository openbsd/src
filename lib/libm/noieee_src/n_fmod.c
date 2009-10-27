/*	$OpenBSD: n_fmod.c,v 1.7 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_fmod.c,v 1.1 1995/10/10 23:36:49 ragge Exp $	*/
/*
 * Copyright (c) 1989, 1993
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

#include "math.h"
#include "mathimpl.h"

/* fmod.c
 *
 * SYNOPSIS
 *
 *    #include <math.h>
 *    double fmod(double x, double y)
 *
 * DESCRIPTION
 *
 *    The fmod function computes the floating-point remainder of x/y.
 *
 * RETURNS
 *
 *    The fmod function returns the value x-i*y, for some integer i
 * such that, if y is nonzero, the result has the same sign as x and
 * magnitude less than the magnitude of y.
 *
 * On a VAX or CCI,
 *
 *    fmod(x,0) traps/faults on floating-point divided-by-zero.
 *
 * On IEEE-754 conforming machines with "isnan()" primitive,
 *
 *    fmod(x,0), fmod(INF,y) are invalid operations and NaN is returned.
 *
 */

double
fmod(double x, double y)
{
	int ir,iy;
	double r,w;

	if (y == (double)0 || isnan(y) || !finite(x))
	    return (x*y)/(x*y);

	r = fabs(x);
	y = fabs(y);
	(void)frexp(y,&iy);
	while (r >= y) {
		(void)frexp(r,&ir);
		w = ldexp(y,ir-iy);
		r -= w <= r ? w : w*(double)0.5;
	}
	return x >= (double)0 ? r : -r;
}

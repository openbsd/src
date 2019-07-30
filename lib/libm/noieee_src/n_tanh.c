/*	$OpenBSD: n_tanh.c,v 1.10 2016/09/12 04:39:47 guenther Exp $	*/
/*	$NetBSD: n_tanh.c,v 1.1 1995/10/10 23:37:08 ragge Exp $	*/
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

#include "math.h"

/* TANH(X)
 * RETURN THE HYPERBOLIC TANGENT OF X
 * DOUBLE PRECISION (VAX D FORMAT 56 BITS, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85;
 * REVISED BY K.C. NG on 2/8/85, 2/11/85, 3/7/85, 3/24/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	isfinite(x)
 *
 * Required kernel function:
 *	expm1(x)	...exp(x)-1
 *
 * Method :
 *	1. reduce x to non-negative by tanh(-x) = - tanh(x).
 *	2.
 *	    0      <  x <=  1.e-10 :  tanh(x) := x
 *					          -expm1(-2x)
 *	    1.e-10 <  x <=  1      :  tanh(x) := --------------
 *					         expm1(-2x) + 2
 *							  2
 *	    1      <= x <=  22.0   :  tanh(x) := 1 -  ---------------
 *						      expm1(2x) + 2
 *	    22.0   <  x <= INF     :  tanh(x) := 1.
 *
 *	Note: 22 was chosen so that fl(1.0+2/(expm1(2*22)+2)) == 1.
 *
 * Special cases:
 *	tanh(NaN) is NaN;
 *	only tanh(0)=0 is exact for finite argument.
 *
 * Accuracy:
 *	tanh(x) returns the exact hyperbolic tangent of x nealy rounded.
 *	In a test run with 1,024,000 random arguments on a VAX, the maximum
 *	observed error was 2.22 ulps (units in the last place).
 */

double
tanh(double x)
{
	static double one=1.0, two=2.0, small = 1.0e-10, big = 1.0e10;
	double t, sign;

	if (isnan(x))
		return (x);

	sign=copysign(one,x);
	x=copysign(x,one);
	if(x < 22.0)
	    if( x > one )
		return(copysign(one-two/(expm1(x+x)+two),sign));
	    else if ( x > small )
		{t= -expm1(-(x+x)); return(copysign(t/(two-t),sign));}
	    else {		/* raise the INEXACT flag for non-zero x */
		t = big + x;
		return(copysign(x,sign) - (t-(big+x)));
	    }
	else if(isfinite(x))
	    return (sign+1.0E-37); /* raise the INEXACT flag */
	else
	    return(sign);	/* x is +- INF */
}

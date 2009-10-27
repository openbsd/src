/*	$OpenBSD: n_expm1.c,v 1.12 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_expm1.c,v 1.1 1995/10/10 23:36:46 ragge Exp $	*/
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

/* EXPM1(X)
 * RETURN THE EXPONENTIAL OF X MINUS ONE
 * DOUBLE PRECISION (IEEE 53 BITS, VAX D FORMAT 56 BITS)
 * CODED IN C BY K.C. NG, 1/19/85;
 * REVISED BY K.C. NG on 2/6/85, 3/7/85, 3/21/85, 4/16/85.
 *
 * Required system supported functions:
 *	scalbn(x,n)
 *	copysign(x,y)
 *	finite(x)
 *
 * Kernel function:
 *	exp__E(x,c)
 *
 * Method:
 *	1. Argument Reduction: given the input x, find r and integer k such
 *	   that
 *	                   x = k*ln2 + r,  |r| <= 0.5*ln2 .
 *	   r will be represented as r := z+c for better accuracy.
 *
 *	2. Compute EXPM1(r)=exp(r)-1 by
 *
 *			EXPM1(r=z+c) := z + exp__E(z,c)
 *
 *	3. EXPM1(x) =  2^k * ( EXPM1(r) + 1-2^-k ).
 *
 * 	Remarks:
 *	   1. When k=1 and z < -0.25, we use the following formula for
 *	      better accuracy:
 *			EXPM1(x) = 2 * ( (z+0.5) + exp__E(z,c) )
 *	   2. To avoid rounding error in 1-2^-k where k is large, we use
 *			EXPM1(x) = 2^k * { [z+(exp__E(z,c)-2^-k )] + 1 }
 *	      when k>56.
 *
 * Special cases:
 *	EXPM1(INF) is INF, EXPM1(NaN) is NaN;
 *	EXPM1(-INF)= -1;
 *	for finite argument, only EXPM1(0)=0 is exact.
 *
 * Accuracy:
 *	EXPM1(x) returns the exact (exp(x)-1) nearly rounded. In a test run with
 *	1,166,000 random arguments on a VAX, the maximum observed error was
 *	.872 ulps (units of the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "math.h"
#include "mathimpl.h"

static const double ln2hi = 6.9314718055829871446E-1;
static const double ln2lo = 1.6465949582897081279E-12;
static const double lnhuge = 9.4961163736712506989E1;
static const double invln2 = 1.4426950408889634148E0;

double
expm1(double x)
{
	static const double one=1.0, half=1.0/2.0, tiny=1e-300;
	double  z,hi,lo,c;
	int k;
#if defined(__vax__)
	static prec=56;
#else	/* defined(__vax__) */
	static prec=53;
#endif	/* defined(__vax__) */

	if (isnan(x))
		return (x);

	if( x <= lnhuge ) {
		if( x >= -40.0 ) {

		    /* argument reduction : x - k*ln2 */
			k= invln2 *x+copysign(0.5,x);	/* k=NINT(x/ln2) */
			hi=x-k*ln2hi ;
			z=hi-(lo=k*ln2lo);
			c=(hi-z)-lo;

			if(k==0) return(z+__exp__E(z,c));
			if(k==1)
			    if(z< -0.25)
				{x=z+half;x +=__exp__E(z,c); return(x+x);}
			    else
				{z+=__exp__E(z,c); x=half+z; return(x+x);}
		    /* end of k=1 */

			else {
			    if(k<=prec)
			      { x=one-scalbn(one,-k); z += __exp__E(z,c);}
			    else if(k<100)
			      { x = __exp__E(z,c)-scalbn(one,-k); x+=z; z=one;}
			    else
			      { x = __exp__E(z,c)+z; z=one;}

			    return (scalbn(x+z,k));
			}
		}
		/* end of x > lnunfl */

		else
		     /* expm1(-big#) rounded to -1 (inexact) */
		     if(finite(x))
			return(tiny-one);

		     /* expm1(-INF) is -1 */
		     else return(-one);
	}
	/* end of x < lnhuge */

	else
	/*  expm1(INF) is INF, expm1(+big#) overflows to INF */
	    return( finite(x) ?  scalbn(one,5000) : x);
}

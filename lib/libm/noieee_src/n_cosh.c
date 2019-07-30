/*	$OpenBSD: n_cosh.c,v 1.10 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_cosh.c,v 1.1 1995/10/10 23:36:42 ragge Exp $	*/
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

/* COSH(X)
 * RETURN THE HYPERBOLIC COSINE OF X
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85;
 * REVISED BY K.C. NG on 2/8/85, 2/23/85, 3/7/85, 3/29/85, 4/16/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	scalbn(x,N)
 *
 * Required kernel function:
 *	exp(x)
 *	exp__E(x,c)	...return exp(x+c)-1-x for |x|<0.3465
 *
 * Method :
 *	1. Replace x by |x|.
 *	2.
 *		                                        [ exp(x) - 1 ]^2
 *	    0        <= x <= 0.3465  :  cosh(x) := 1 + -------------------
 *			       			           2*exp(x)
 *
 *		                                   exp(x) +  1/exp(x)
 *	    0.3465   <= x <= 22      :  cosh(x) := -------------------
 *			       			           2
 *	    22       <= x <= lnovfl  :  cosh(x) := exp(x)/2
 *	    lnovfl   <= x <= lnovfl+log(2)
 *				     :  cosh(x) := exp(x)/2 (avoid overflow)
 *	    log(2)+lnovfl <  x <  INF:  overflow to INF
 *
 *	Note: .3465 is a number near one half of ln2.
 *
 * Special cases:
 *	cosh(x) is x if x is +INF, -INF, or NaN.
 *	only cosh(0)=1 is exact for finite x.
 *
 * Accuracy:
 *	cosh(x) returns the exact hyperbolic cosine of x nearly rounded.
 *	In a test run with 768,000 random arguments on a VAX, the maximum
 *	observed error was 1.23 ulps (units in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "math.h"
#include "mathimpl.h"

static const double mln2hi = 8.8029691931113054792E1;
static const double mln2lo = -4.9650192275318476525E-16;
static const double lnovfl = 8.8029691931113053016E1;

#if defined(__vax__)
static max = 126                      ;
#else	/* defined(__vax__) */
static max = 1023                     ;
#endif	/* defined(__vax__) */

double
cosh(double x)
{
	static const double half=1.0/2.0,
		one=1.0, small=1.0E-18; /* fl(1+small)==1 */
	double t;

	if (isnan(x))
		return (x);

	if((x=copysign(x,one)) <= 22)
	    if(x<0.3465)
		if(x<small) return(one+x);
		else {t=x+__exp__E(x,0.0);x=t+t; return(one+t*t/(2.0+x)); }

	    else /* for x lies in [0.3465,22] */
	        { t=exp(x); return((t+one/t)*half); }

	if( lnovfl <= x && x <= (lnovfl+0.7))
        /* for x lies in [lnovfl, lnovfl+ln2], decrease x by ln(2^(max+1))
         * and return 2^max*exp(x) to avoid unnecessary overflow
         */
	    return(scalbn(exp((x-mln2hi)-mln2lo), max));

	else
	    return(exp(x)*half);	/* for large x,  cosh(x)=exp(x)/2 */
}

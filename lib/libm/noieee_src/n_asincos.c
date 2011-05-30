/*	$OpenBSD: n_asincos.c,v 1.11 2011/05/30 18:34:38 martynas Exp $	*/
/*	$NetBSD: n_asincos.c,v 1.1 1995/10/10 23:36:34 ragge Exp $	*/
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

/* ASIN(X)
 * RETURNS ARC SINE OF X
 * DOUBLE PRECISION (IEEE DOUBLE 53 bits, VAX D FORMAT 56 bits)
 * CODED IN C BY K.C. NG, 4/16/85, REVISED ON 6/10/85.
 *
 * Required system supported functions:
 *	copysign(x,y)
 *	sqrt(x)
 *
 * Required kernel function:
 *	atan2(y,x)
 *
 * Method:
 *	asin(x) = atan2(x,sqrt(1-x*x)); for better accuracy, 1-x*x is
 *		  computed as follows
 *			1-x*x                     if x <  0.5,
 *			2*(1-|x|)-(1-|x|)*(1-|x|) if x >= 0.5.
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN.
 *
 * Accuracy:
 * 1)  If atan2() uses machine PI, then
 *
 *	asin(x) returns (PI/pi) * (the exact arc sine of x) nearly rounded;
 *	and PI is the exact pi rounded to machine precision (see atan2 for
 *      details):
 *
 *	in decimal:
 *		pi = 3.141592653589793 23846264338327 .....
 *    53 bits   PI = 3.141592653589793 115997963 ..... ,
 *    56 bits   PI = 3.141592653589793 227020265 ..... ,
 *
 *	in hexadecimal:
 *		pi = 3.243F6A8885A308D313198A2E....
 *    53 bits   PI = 3.243F6A8885A30  =  2 * 1.921FB54442D18	error=.276ulps
 *    56 bits   PI = 3.243F6A8885A308 =  4 * .C90FDAA22168C2    error=.206ulps
 *
 *	In a test run with more than 200,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *	2.06 ulps.      (comparing against (PI/pi)*(exact asin(x)));
 *
 * 2)  If atan2() uses true pi, then
 *
 *	asin(x) returns the exact asin(x) with error below about 2 ulps.
 *
 *	In a test run with more than 1,024,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *      1.99 ulps.
 */

/* LINTLIBRARY */

#include <sys/cdefs.h>
#include <math.h>

#include "mathimpl.h"

double
asin(double x)
{
	double s, t, one = 1.0;

	if (isnan(x))
		return (x);

	s=copysign(x,one);
	if(s <= 0.5)
	    return(atan2(x,sqrt(one-x*x)));
	else
	    { t=one-s; s=t+t; return(atan2(x,sqrt(s-t*t))); }

}

#ifdef	lint
/* PROTOLIB1 */
long double asinl(long double);
/* PROTOLIB0 */
#else	/* lint */
__weak_alias(asinl, asin);
#endif	/* lint */

/* ACOS(X)
 * RETURNS ARC COS OF X
 * DOUBLE PRECISION (IEEE DOUBLE 53 bits, VAX D FORMAT 56 bits)
 * CODED IN C BY K.C. NG, 4/16/85, REVISED ON 6/10/85.
 *
 * Required system supported functions:
 *	copysign(x,y)
 *	sqrt(x)
 *
 * Required kernel function:
 *	atan2(y,x)
 *
 * Method:
 *			      ________
 *                           / 1 - x
 *	acos(x) = 2*atan2(  / -------- , 1 ) .
 *                        \/   1 + x
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN.
 *
 * Accuracy:
 * 1)  If atan2() uses machine PI, then
 *
 *	acos(x) returns (PI/pi) * (the exact arc cosine of x) nearly rounded;
 *	and PI is the exact pi rounded to machine precision (see atan2 for
 *      details):
 *
 *	in decimal:
 *		pi = 3.141592653589793 23846264338327 .....
 *    53 bits   PI = 3.141592653589793 115997963 ..... ,
 *    56 bits   PI = 3.141592653589793 227020265 ..... ,
 *
 *	in hexadecimal:
 *		pi = 3.243F6A8885A308D313198A2E....
 *    53 bits   PI = 3.243F6A8885A30  =  2 * 1.921FB54442D18	error=.276ulps
 *    56 bits   PI = 3.243F6A8885A308 =  4 * .C90FDAA22168C2    error=.206ulps
 *
 *	In a test run with more than 200,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *	2.07 ulps.      (comparing against (PI/pi)*(exact acos(x)));
 *
 * 2)  If atan2() uses true pi, then
 *
 *	acos(x) returns the exact acos(x) with error below about 2 ulps.
 *
 *	In a test run with more than 1,024,000 random arguments on a VAX, the
 *	maximum observed error in ulps (units in the last place) was
 *	2.15 ulps.
 */

double
acos(double x)
{
	double t, one = 1.0;

	if (isnan(x))
		return (x);

	if( x != -1.0)
	    t=atan2(sqrt((one-x)/(one+x)),one);
	else
	    t=atan2(one,0.0);	/* t = PI/2 */
	return(t+t);
}

#ifdef	lint
/* PROTOLIB1 */
long double acosl(long double);
/* PROTOLIB0 */
#else	/* lint */
__weak_alias(acosl, acos);
#endif	/* lint */

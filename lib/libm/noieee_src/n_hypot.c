/*	$OpenBSD: n_hypot.c,v 1.3 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_cabs.c,v 1.1 1995/10/10 23:36:39 ragge Exp $	*/
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

/* HYPOT(X,Y)
 * RETURN THE SQUARE ROOT OF X^2 + Y^2  WHERE Z=X+iY
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 11/28/84;
 * REVISED BY K.C. NG, 7/12/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	finite(x)
 *	scalbn(x,N)
 *	sqrt(x)
 *
 * Method :
 *	1. replace x by |x| and y by |y|, and swap x and
 *	   y if y > x (hence x is never smaller than y).
 *	2. Hypot(x,y) is computed by:
 *	   Case I, x/y > 2
 *
 *				       y
 *		hypot = x + -----------------------------
 *			 		    2
 *			    sqrt ( 1 + [x/y]  )  +  x/y
 *
 *	   Case II, x/y <= 2
 *				                   y
 *		hypot = x + --------------------------------------------------
 *				          		     2
 *				     			[x/y]   -  2
 *			   (sqrt(2)+1) + (x-y)/y + -----------------------------
 *			 		    			  2
 *			    			  sqrt ( 1 + [x/y]  )  + sqrt(2)
 *
 *
 *
 * Special cases:
 *	hypot(x,y) is INF if x or y is +INF or -INF; else
 *	hypot(x,y) is NAN if x or y is NAN.
 *
 * Accuracy:
 * 	hypot(x,y) returns the sqrt(x^2+y^2) with error less than 1 ulps (units
 *	in the last place). See Kahan's "Interval Arithmetic Options in the
 *	Proposed IEEE Floating Point Arithmetic Standard", Interval Mathematics
 *      1980, Edited by Karl L.E. Nickel, pp 99-128. (A faster but less accurate
 *	code follows in	comments.) In a test run with 500,000 random arguments
 *	on a VAX, the maximum observed error was .959 ulps.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "math.h"
#include "mathimpl.h"

static const double r2p1hi = 2.4142135623730950345E0;
static const double r2p1lo = 1.4349369327986523769E-17;
static const double sqrt2 = 1.4142135623730950622E0;

double
hypot(double x, double y)
{
	static const double zero=0, one=1,
		      small=1.0E-18;	/* fl(1+small)==1 */
	static const ibig=30;	/* fl(1+2**(2*ibig))==1 */
	double t,r;
	int exp;

	if(finite(x))
	    if(finite(y))
	    {
		x=copysign(x,one);
		y=copysign(y,one);
		if(y > x)
		    { t=x; x=y; y=t; }
		if(x == zero) return(zero);
		if(y == zero) return(x);
		exp= logb(x);
		if (exp - (int)logb(y) > ibig) {
			if (one + small >= 1.0)	/* raise inexact flag */
				return(x);	/* return |x| */
		}

	    /* start computing sqrt(x^2 + y^2) */
		r=x-y;
		if(r>y) { 	/* x/y > 2 */
		    r=x/y;
		    r=r+sqrt(one+r*r); }
		else {		/* 1 <= x/y <= 2 */
		    r/=y; t=r*(r+2.0);
		    r+=t/(sqrt2+sqrt(2.0+t));
		    r+=r2p1lo; r+=r2p1hi; }

		r=y/r;
		return(x+r);

	    }

	    else if(isinf(y))		/* y is +-INF */
		     return(copysign(y,one));
	    else
		     return(y);		/* y is NaN and x is finite */

	else if(isinf(x))		/* x is +-INF */
	         return (copysign(x,one));
	else if(finite(y))
	         return(x);		/* x is NaN, y is finite */
	else if (isnan(y))
		return (y);
	else return(copysign(y,one));	/* y is INF */
}

/* A faster but less accurate version of cabs(x,y) */
#if 0
double
hypot(double x, double y)
{
	static const double zero=0, one=1;
		      small=1.0E-18;	/* fl(1+small)==1 */
	static const ibig=30;	/* fl(1+2**(2*ibig))==1 */
	double temp;
	int exp;

	if(finite(x))
	    if(finite(y))
	    {
		x=copysign(x,one);
		y=copysign(y,one);
		if(y > x)
		    { temp=x; x=y; y=temp; }
		if(x == zero) return(zero);
		if(y == zero) return(x);
		exp= logb(x);
		x=scalbn(x,-exp);
		if (exp - (int)logb(y) > ibig) {
			if (one + small >= 1.0)		/* raise inexact flag */
				return(scalbn(x,exp));	/* return |x| */
		}
		else y=scalbn(y,-exp);
		return(scalbn(sqrt(x*x+y*y),exp));
	    }

	    else if(isinf(y))		/* y is +-INF */
		     return(copysign(y,one));
	    else
		     return(y);		/* y is NaN and x is finite */

	else if(isinf(x))		/* x is +-INF */
	         return (copysign(x,one));
	else if(finite(y))
	         return(x);		/* x is NaN, y is finite */
	else if(isnan(y)) return(y);	/* x and y is NaN */
	else return(copysign(y,one));	/* y is INF */
}
#endif

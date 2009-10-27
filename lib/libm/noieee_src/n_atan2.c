/*	$OpenBSD: n_atan2.c,v 1.13 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_atan2.c,v 1.1 1995/10/10 23:36:37 ragge Exp $	*/
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

/* ATAN2(Y,X)
 * RETURN ARG (X+iY)
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/8/85;
 * REVISED BY K.C. NG on 2/7/85, 2/13/85, 3/7/85, 3/30/85, 6/29/85.
 *
 * Required system supported functions :
 *	copysign(x,y)
 *	scalbn(x,y)
 *	logb(x)
 *
 * Method :
 *	1. Reduce y to positive by atan2(y,x)=-atan2(-y,x).
 *	2. Reduce x to positive by (if x and y are unexceptional):
 *		ARG (x+iy) = arctan(y/x)   	   ... if x > 0,
 *		ARG (x+iy) = pi - arctan[y/(-x)]   ... if x < 0,
 *	3. According to the integer k=4t+0.25 truncated , t=y/x, the argument
 *	   is further reduced to one of the following intervals and the
 *	   arctangent of y/x is evaluated by the corresponding formula:
 *
 *         [0,7/16]	   atan(y/x) = t - t^3*(a1+t^2*(a2+...(a10+t^2*a11)...)
 *	   [7/16,11/16]    atan(y/x) = atan(1/2) + atan( (y-x/2)/(x+y/2) )
 *	   [11/16.19/16]   atan(y/x) = atan( 1 ) + atan( (y-x)/(x+y) )
 *	   [19/16,39/16]   atan(y/x) = atan(3/2) + atan( (y-1.5x)/(x+1.5y) )
 *	   [39/16,INF]     atan(y/x) = atan(INF) + atan( -x/y )
 *
 * Special cases:
 * Notations: atan2(y,x) == ARG (x+iy) == ARG(x,y).
 *
 *	ARG( NAN , (anything) ) is NaN;
 *	ARG( (anything), NaN ) is NaN;
 *	ARG(+(anything but NaN), +-0) is +-0  ;
 *	ARG(-(anything but NaN), +-0) is +-PI ;
 *	ARG( 0, +-(anything but 0 and NaN) ) is +-PI/2;
 *	ARG( +INF,+-(anything but INF and NaN) ) is +-0 ;
 *	ARG( -INF,+-(anything but INF and NaN) ) is +-PI;
 *	ARG( +INF,+-INF ) is +-PI/4 ;
 *	ARG( -INF,+-INF ) is +-3PI/4;
 *	ARG( (anything but,0,NaN, and INF),+-INF ) is +-PI/2;
 *
 * Accuracy:
 *	atan2(y,x) returns (PI/pi) * the exact ARG (x+iy) nearly rounded,
 *	where
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
 *	In a test run with 356,000 random argument on [-1,1] * [-1,1] on a
 *	VAX, the maximum observed error was 1.41 ulps (units of the last place)
 *	compared with (PI/pi)*(the exact ARG(x+iy)).
 *
 * Note:
 *	We use machine PI (the true pi rounded) in place of the actual
 *	value of pi for all the trig and inverse trig functions. In general,
 *	if trig is one of sin, cos, tan, then computed trig(y) returns the
 *	exact trig(y*pi/PI) nearly rounded; correspondingly, computed arctrig
 *	returns the exact arctrig(y)*PI/pi nearly rounded. These guarantee the
 *	trig functions have period PI, and trig(arctrig(x)) returns x for
 *	all critical values x.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include <sys/cdefs.h>
#include <math.h>

#include "mathimpl.h"

static const double athfhi = 4.6364760900080611433E-1;
static const double athflo = 1.9338828231967579916E-19;
static const double PIo4 = 7.8539816339744830676E-1;
static const double at1fhi = 9.8279372324732906796E-1;
static const double at1flo = -3.5540295636764633916E-18;
static const double PIo2 = 1.5707963267948966135E0;
static const double PI = 3.1415926535897932270E0;
static const double a1 = 3.3333333333333473730E-1;
static const double a2 = -2.0000000000017730678E-1;
static const double a3 = 1.4285714286694640301E-1;
static const double a4 = -1.1111111135032672795E-1;
static const double a5 = 9.0909091380563043783E-2;
static const double a6 = -7.6922954286089459397E-2;
static const double a7 = 6.6663180891693915586E-2;
static const double a8 = -5.8772703698290408927E-2;
static const double a9 = 5.2170707402812969804E-2;
static const double a10 = -4.4895863157820361210E-2;
static const double a11 = 3.3006147437343875094E-2;
static const double a12 = -1.4614844866464185439E-2;

double
atan2(double y, double x)
{
	static const double zero=0, one=1, small=1.0E-9, big=1.0E18;
	double t,z,signy,signx,hi,lo;
	int k,m;

    /* if x or y is NAN */
	if (isnan(x))
		return (x);
	if (isnan(y))
		return (y);

    /* copy down the sign of y and x */
	signy = copysign(one,y) ;
	signx = copysign(one,x) ;

    /* if x is 1.0, goto begin */
	if(x==1) { y=copysign(y,one); t=y; if(finite(t)) goto begin;}

    /* when y = 0 */
	if(y==zero) return((signx==one)?y:copysign(PI,signy));

    /* when x = 0 */
	if(x==zero) return(copysign(PIo2,signy));

    /* when x is INF */
	if(!finite(x))
	    if(!finite(y))
		return(copysign((signx==one)?PIo4:3*PIo4,signy));
	    else
		return(copysign((signx==one)?zero:PI,signy));

    /* when y is INF */
	if(!finite(y)) return(copysign(PIo2,signy));

    /* compute y/x */
	x=copysign(x,one);
	y=copysign(y,one);
	if((m=(k=logb(y))-logb(x)) > 60) t=big+big;
	    else if(m < -80 ) t=y/x;
	    else { t = y/x ; y = scalbn(y,-k); x=scalbn(x,-k); }

    /* begin argument reduction */
begin:
	if (t < 2.4375) {

	/* truncate 4(t+1/16) to integer for branching */
	    k = 4 * (t+0.0625);
	    switch (k) {

	    /* t is in [0,7/16] */
	    case 0:
	    case 1:
		if (t < small) {
			if (big + small > 0.0)	/* raise inexact flag */
				return (copysign((signx>zero)?t:PI-t,signy));
		}

		hi = zero;  lo = zero;  break;

	    /* t is in [7/16,11/16] */
	    case 2:
		hi = athfhi; lo = athflo;
		z = x+x;
		t = ( (y+y) - x ) / ( z +  y ); break;

	    /* t is in [11/16,19/16] */
	    case 3:
	    case 4:
		hi = PIo4; lo = zero;
		t = ( y - x ) / ( x + y ); break;

	    /* t is in [19/16,39/16] */
	    default:
		hi = at1fhi; lo = at1flo;
		z = y-x; y=y+y+y; t = x+x;
		t = ( (z+z)-x ) / ( t + y ); break;
	    }
	}
	/* end of if (t < 2.4375) */

	else
	{
	    hi = PIo2; lo = zero;

	    /* t is in [2.4375, big] */
	    if (t <= big)  t = - x / y;

	    /* t is in [big, INF] */
	    else {
		if (big + small > 0.0)	/* raise inexact flag */
			t = zero;
	    }
	}
    /* end of argument reduction */

    /* compute atan(t) for t in [-.4375, .4375] */
	z = t*t;
#if defined(__vax__)
	z = t*(z*(a1+z*(a2+z*(a3+z*(a4+z*(a5+z*(a6+z*(a7+z*(a8+
			z*(a9+z*(a10+z*(a11+z*a12))))))))))));
#else	/* defined(__vax__) */
	z = t*(z*(a1+z*(a2+z*(a3+z*(a4+z*(a5+z*(a6+z*(a7+z*(a8+
			z*(a9+z*(a10+z*a11)))))))))));
#endif	/* defined(__vax__) */
	z = lo - z; z += t; z += hi;

	return(copysign((signx>zero)?z:PI-z,signy));
}

#ifdef __weak_alias
__weak_alias(atan2l, atan2);
#endif /* __weak_alias */

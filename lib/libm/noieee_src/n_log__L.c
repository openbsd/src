/*	$OpenBSD: n_log__L.c,v 1.9 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_log__L.c,v 1.1 1995/10/10 23:37:01 ragge Exp $	*/
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

/* log__L(Z)
 *		LOG(1+X) - 2S			       X
 * RETURN      ---------------  WHERE Z = S*S,  S = ------- , 0 <= Z <= .0294...
 *		      S				     2 + X
 *
 * DOUBLE PRECISION (VAX D FORMAT 56 bits or IEEE DOUBLE 53 BITS)
 * KERNEL FUNCTION FOR LOG; TO BE USED IN LOG1P, LOG, AND POW FUNCTIONS
 * CODED IN C BY K.C. NG, 1/19/85;
 * REVISED BY K.C. Ng, 2/3/85, 4/16/85.
 *
 * Method :
 *	1. Polynomial approximation: let s = x/(2+x).
 *	   Based on log(1+x) = log(1+s) - log(1-s)
 *		 = 2s + 2/3 s**3 + 2/5 s**5 + .....,
 *
 *	   (log(1+x) - 2s)/s is computed by
 *
 *	       z*(L1 + z*(L2 + z*(... (L7 + z*L8)...)))
 *
 *	   where z=s*s. (See the listing below for Lk's values.) The
 *	   coefficients are obtained by a special Remes algorithm.
 *
 * Accuracy:
 *	Assuming no rounding error, the maximum magnitude of the approximation
 *	error (absolute) is 2**(-58.49) for IEEE double, and 2**(-63.63)
 *	for VAX D format.
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "math.h"
#include "mathimpl.h"

static const double L1 = 6.6666666666666703212E-1;
static const double L2 = 3.9999999999970461961E-1;
static const double L3 = 2.8571428579395698188E-1;
static const double L4 = 2.2222221233634724402E-1;
static const double L5 = 1.8181879517064680057E-1;
static const double L6 = 1.5382888777946145467E-1;
static const double L7 = 1.3338356561139403517E-1;
static const double L8 = 1.2500000000000000000E-1;

double
__log__L(double z)
{
#if defined(__vax__)
    return(z*(L1+z*(L2+z*(L3+z*(L4+z*(L5+z*(L6+z*(L7+z*L8))))))));
#else	/* defined(__vax__) */
    return(z*(L1+z*(L2+z*(L3+z*(L4+z*(L5+z*(L6+z*L7)))))));
#endif	/* defined(__vax__) */
}

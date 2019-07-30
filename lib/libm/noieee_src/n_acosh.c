/*	$OpenBSD: n_acosh.c,v 1.9 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_acosh.c,v 1.1 1995/10/10 23:36:33 ragge Exp $	*/
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

/* ACOSH(X)
 * RETURN THE INVERSE HYPERBOLIC COSINE OF X
 * DOUBLE PRECISION (VAX D FORMAT 56 BITS, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 2/16/85;
 * REVISED BY K.C. NG on 3/6/85, 3/24/85, 4/16/85, 8/17/85.
 *
 * Required system supported functions :
 *	sqrt(x)
 *
 * Required kernel function:
 *	log1p(x) 		...return log(1+x)
 *
 * Method :
 *	Based on
 *		acosh(x) = log [ x + sqrt(x*x-1) ]
 *	we have
 *		acosh(x) := log1p(x)+ln2,	if (x > 1.0E20); else
 *		acosh(x) := log1p( sqrt(x-1) * (sqrt(x-1) + sqrt(x+1)) ) .
 *	These formulae avoid the over/underflow complication.
 *
 * Special cases:
 *	acosh(x) is NaN with signal if x<1.
 *	acosh(NaN) is NaN without signal.
 *
 * Accuracy:
 *	acosh(x) returns the exact inverse hyperbolic cosine of x nearly
 *	rounded. In a test run with 512,000 random arguments on a VAX, the
 *	maximum observed error was 3.30 ulps (units of the last place) at
 *	x=1.0070493753568216 .
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

double
acosh(double x)
{
	double t,big=1.E20; /* big+1==big */

	if (isnan(x))
		return (x);

    /* return log1p(x) + log(2) if x is large */
	if(x>big) {t=log1p(x)+ln2lo; return(t+ln2hi);}

	t=sqrt(x-1.0);
	return(log1p(t*(t+sqrt(x+1.0))));
}

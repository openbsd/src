/*	$OpenBSD: n_log10.c,v 1.8 2009/10/27 23:59:29 deraadt Exp $	*/
/*	$NetBSD: n_log10.c,v 1.1 1995/10/10 23:36:58 ragge Exp $	*/
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

/* LOG10(X)
 * RETURN THE BASE 10 LOGARITHM OF x
 * DOUBLE PRECISION (VAX D format 56 bits, IEEE DOUBLE 53 BITS)
 * CODED IN C BY K.C. NG, 1/20/85;
 * REVISED BY K.C. NG on 1/23/85, 3/7/85, 4/16/85.
 *
 * Required kernel function:
 *	log(x)
 *
 * Method :
 *			     log(x)
 *		log10(x) = ---------  or  [1/log(10)]*log(x)
 *			    log(10)
 *
 *    Note:
 *	  [log(10)]   rounded to 56 bits has error  .0895  ulps,
 *	  [1/log(10)] rounded to 53 bits has error  .198   ulps;
 *	  therefore, for better accuracy, in VAX D format, we divide
 *	  log(x) by log(10), but in IEEE Double format, we multiply
 *	  log(x) by [1/log(10)].
 *
 * Special cases:
 *	log10(x) is NaN with signal if x < 0;
 *	log10(+INF) is +INF with no signal; log10(0) is -INF with signal;
 *	log10(NaN) is that NaN with no signal.
 *
 * Accuracy:
 *	log10(X) returns the exact log10(x) nearly rounded. In a test run
 *	with 1,536,000 random arguments on a VAX, the maximum observed
 *	error was 1.74 ulps (units in the last place).
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following constants.
 * The decimal values may be used, provided that the compiler will convert
 * from decimal to binary accurately enough to produce the hexadecimal values
 * shown.
 */

#include "math.h"
#include "mathimpl.h"

static const double ln10hi = 2.3025850929940456790E0;

double
log10(double x)
{
#if defined(__vax__)
	return(log(x)/ln10hi);
#else	/* defined(__vax__) */
	return(ivln10*log(x));
#endif	/* defined(__vax__) */
}

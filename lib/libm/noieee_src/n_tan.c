/*	$OpenBSD: n_tan.c,v 1.16 2016/09/12 19:47:02 guenther Exp $	*/
/*	$NetBSD: n_tan.c,v 1.1 1995/10/10 23:37:07 ragge Exp $	*/
/*
 * Copyright (c) 1987, 1993
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

#include <math.h>

#include "mathimpl.h"

float
tanf(float x)
{
	return (float)tan((double) x);
}

double
tan(double x)
{
	double a,z,ss,cc,c;
	int k;

	if(!isfinite(x))	/* tan(NaN) and tan(INF) must be NaN */
		return x-x;
	x = remainder(x,PI);	/* reduce x into [-PI/2, PI/2] */
	a = copysign(x,one);	/* ... = abs(x) */
	if (a >= PIo4) {
		k = 1;
		x = copysign(PIo2-a,x);
	}
	else {
		k = 0;
		if (a < small) {
			big+a;
			return x;
		}
	}
	z = x*x;
	cc = cos__C(z);
	ss = sin__S(z);
	z *= half;			/* Next get c = cos(x) accurately */
	c = (z >= thresh ? half-((z-half)-cc) : one-(z-cc));
	if (k == 0)
		return x+(x*(z-(cc-ss)))/c;	/* ... sin/cos */
	else
		return c/(x+x*ss);		/* ... cos/sin */
}
DEF_STD(tan);
LDBL_UNUSED_CLONE(tan);

/*	$OpenBSD: fpu_rem.c,v 1.4 2006/01/16 22:08:26 miod Exp $	*/
/*	$NetBSD: fpu_rem.c,v 1.5 2003/07/15 02:43:10 lukem Exp $	*/

/*
 * Copyright (c) 1995  Ken Nakata
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)fpu_rem.c	10/24/95
 */

#include <sys/types.h>
#include <sys/signal.h>
#include <machine/frame.h>

#include "fpu_emulate.h"

/*
 *       ALGORITHM
 *
 *       Step 1.  Save and strip signs of X and Y: signX := sign(X),
 *                signY := sign(Y), X := *X*, Y := *Y*, 
 *                signQ := signX EOR signY. Record whether MOD or REM
 *                is requested.
 *
 *       Step 2.  Set L := expo(X)-expo(Y), k := 0, Q := 0.
 *                If (L < 0) then
 *                   R := X, go to Step 4.
 *                else
 *                   R := 2^(-L)X, j := L.
 *                endif
 *
 *       Step 3.  Perform MOD(X,Y)
 *            3.1 If R = Y, go to Step 9.
 *            3.2 If R > Y, then { R := R - Y, Q := Q + 1}
 *            3.3 If j = 0, go to Step 4.
 *            3.4 k := k + 1, j := j - 1, Q := 2Q, R := 2R. Go to
 *                Step 3.1.
 *
 *       Step 4.  At this point, R = X - QY = MOD(X,Y). Set
 *                Last_Subtract := false (used in Step 7 below). If
 *                MOD is requested, go to Step 6. 
 *
 *       Step 5.  R = MOD(X,Y), but REM(X,Y) is requested.
 *            5.1 If R < Y/2, then R = MOD(X,Y) = REM(X,Y). Go to
 *                Step 6.
 *            5.2 If R > Y/2, then { set Last_Subtract := true,
 *                Q := Q + 1, Y := signY*Y }. Go to Step 6.
 *            5.3 This is the tricky case of R = Y/2. If Q is odd,
 *                then { Q := Q + 1, signX := -signX }.
 *
 *       Step 6.  R := signX*R.
 *
 *       Step 7.  If Last_Subtract = true, R := R - Y.
 *
 *       Step 8.  Return signQ, last 7 bits of Q, and R as required.
 *
 *       Step 9.  At this point, R = 2^(-j)*X - Q Y = Y. Thus,
 *                X = 2^(j)*(Q+1)Y. set Q := 2^(j)*(Q+1),
 *                R := 0. Return signQ, last 7 bits of Q, and R.
 */                

struct fpn *__fpu_modrem(struct fpemu *fe, int modrem);

struct fpn *
__fpu_modrem(fe, modrem)
     struct fpemu *fe;
     int modrem;
{
    static struct fpn X, Y;
    struct fpn *x, *y, *r;
    u_int signX, signY, signQ;
    int j, k, l, q;
    int Last_Subtract;

    CPYFPN(&X, &fe->fe_f1);
    CPYFPN(&Y, &fe->fe_f2);
    x = &X;
    y = &Y;
    r = &fe->fe_f2;

    /*
     * Step 1
     */
    signX = x->fp_sign;
    signY = y->fp_sign;
    signQ = (signX ^ signY);
    x->fp_sign = y->fp_sign = 0;

    /*
     * Step 2
     */
    l = x->fp_exp - y->fp_exp;
    k = 0;
    q = 0;
    if (l >= 0) {
	CPYFPN(r, x);
	r->fp_exp -= l;
	j = l;

	/*
	 * Step 3
	 */
	while (y->fp_exp != r->fp_exp || y->fp_mant[0] != r->fp_mant[0] ||
	       y->fp_mant[1] != r->fp_mant[1] ||
	       y->fp_mant[2] != r->fp_mant[2]) {

	    /* Step 3.2 */
	    if (y->fp_exp < r->fp_exp || y->fp_mant[0] < r->fp_mant[0] ||
		y->fp_mant[1] < r->fp_mant[1] ||
		y->fp_mant[2] < r->fp_mant[2]) {
		CPYFPN(&fe->fe_f1, r);
		CPYFPN(&fe->fe_f2, y);
		fe->fe_f2.fp_sign = 1;
		r = fpu_add(fe);
		q++;
	    }

	    /* Step 3.3 */
	    if (j == 0)
		goto Step4;

	    /* Step 3.4 */
	    k++;
	    j--;
	    q += q;
	    r->fp_exp++;
	}
	/* Step 9 */
	goto Step9;
    }
 Step4:
    Last_Subtract = 0;
    if (modrem == 0)
	goto Step6;

    /*
     * Step 5
     */
    /* Step 5.1 */
    if (r->fp_exp + 1 < y->fp_exp ||
	(r->fp_exp + 1 == y->fp_exp &&
	 (r->fp_mant[0] < y->fp_mant[0] || r->fp_mant[1] < y->fp_mant[1] ||
	  r->fp_mant[2] < y->fp_mant[2])))
	/* if r < y/2 */
	goto Step6;
    /* Step 5.2 */
    if (r->fp_exp + 1 != y->fp_exp ||
	r->fp_mant[0] != y->fp_mant[0] || r->fp_mant[1] != y->fp_mant[1] ||
	r->fp_mant[2] != y->fp_mant[2]) {
	/* if (!(r < y/2) && !(r == y/2)) */
	Last_Subtract = 1;
	q++;
	y->fp_sign = signY;
    } else {
	/* Step 5.3 */
	/* r == y/2 */
	if (q % 2) {
	    q++;
	    signX = !signX;
	}
    }

 Step6:
    r->fp_sign = signX;

    /*
     * Step 7
     */
    if (Last_Subtract) {
	CPYFPN(&fe->fe_f1, r);
	CPYFPN(&fe->fe_f2, y);
	fe->fe_f2.fp_sign = !y->fp_sign;
	r = fpu_add(fe);
    }
    /*
     * Step 8
     */
    q &= 0x7f;
    q |= (signQ << 7);
    fe->fe_fpframe->fpf_fpsr =
	fe->fe_fpsr =
	    (fe->fe_fpsr & ~FPSR_QTT) | (q << 16);
    return r;

 Step9:
    fe->fe_f1.fp_class = FPC_ZERO;
    q++;
    q &= 0x7f;
    q |= (signQ << 7);
    fe->fe_fpframe->fpf_fpsr =
	fe->fe_fpsr =
	    (fe->fe_fpsr & ~FPSR_QTT) | (q << 16);
    return &fe->fe_f1;
}

struct fpn *
fpu_rem(fe)
     struct fpemu *fe;
{
  return __fpu_modrem(fe, 1);
}

struct fpn *
fpu_mod(fe)
     struct fpemu *fe;
{
  return __fpu_modrem(fe, 0);
}

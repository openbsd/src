/*	$OpenBSD: sfsqrt.c,v 1.4 2000/01/11 08:18:43 mickey Exp $	*/

/*
 * Copyright 1996 1995 by Open Software Foundation, Inc.   
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * pmk1.1
 */
/*
 * (c) Copyright 1986 HEWLETT-PACKARD COMPANY
 *
 * To anyone who acknowledges that this file is provided "AS IS" 
 * without any express or implied warranty:
 *     permission to use, copy, modify, and distribute this file 
 * for any purpose is hereby granted without fee, provided that 
 * the above copyright notice and this notice appears in all 
 * copies, and that the name of Hewlett-Packard Company not be 
 * used in advertising or publicity pertaining to distribution 
 * of the software without specific, written prior permission.  
 * Hewlett-Packard Company makes no representations about the 
 * suitability of this software for any purpose.
 */

#include "../spmath/float.h"
#include "../spmath/sgl_float.h"

/*
 *  Single Floating-point Square Root
 */

/*ARGSUSED*/
int
sgl_fsqrt(srcptr,dstptr,status)

sgl_floating_point *srcptr, *dstptr;
unsigned int *status;
{
	register unsigned int src, result;
	register int src_exponent, newbit, sum;
	register boolean guardbit = FALSE, even_exponent;

	src = *srcptr;
        /*
         * check source operand for NaN or infinity
         */
        if ((src_exponent = Sgl_exponent(src)) == SGL_INFINITY_EXPONENT) {
                /*
                 * is signaling NaN?
                 */
                if (Sgl_isone_signaling(src)) {
                        /* trap if INVALIDTRAP enabled */
                        if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
                        /* make NaN quiet */
                        Set_invalidflag();
                        Sgl_set_quiet(src);
                }
                /*
                 * Return quiet NaN or positive infinity.
		 *  Fall thru to negative test if negative infinity.
                 */
		if (Sgl_iszero_sign(src) || Sgl_isnotzero_mantissa(src)) {
                	*dstptr = src;
                	return(NOEXCEPTION);
		}
        }

        /*
         * check for zero source operand
         */
	if (Sgl_iszero_exponentmantissa(src)) {
		*dstptr = src;
		return(NOEXCEPTION);
	}

        /*
         * check for negative source operand 
         */
	if (Sgl_isone_sign(src)) {
		/* trap if INVALIDTRAP enabled */
		if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);
		/* make NaN quiet */
		Set_invalidflag();
		Sgl_makequietnan(src);
		*dstptr = src;
		return(NOEXCEPTION);
	}

	/*
	 * Generate result
	 */
	if (src_exponent > 0) {
		even_exponent = Sgl_hidden(src);
		Sgl_clear_signexponent_set_hidden(src);
	}
	else {
		/* normalize operand */
		Sgl_clear_signexponent(src);
		src_exponent++;
		Sgl_normalize(src,src_exponent);
		even_exponent = src_exponent & 1;
	}
	if (even_exponent) {
		/* exponent is even */
		/* Add comment here.  Explain why odd exponent needs correction */
		Sgl_leftshiftby1(src);
	}
	/*
	 * Add comment here.  Explain following algorithm.
	 * 
	 * Trust me, it works.
	 *
	 */
	Sgl_setzero(result);
	newbit = 1 << SGL_P;
	while (newbit && Sgl_isnotzero(src)) {
		Sgl_addition(result,newbit,sum);
		if(sum <= Sgl_all(src)) {
			/* update result */
			Sgl_addition(result,(newbit<<1),result);
			Sgl_subtract(src,sum,src);
		}
		Sgl_rightshiftby1(newbit);
		Sgl_leftshiftby1(src);
	}
	/* correct exponent for pre-shift */
	if (even_exponent) {
		Sgl_rightshiftby1(result);
	}

	/* check for inexact */
	if (Sgl_isnotzero(src)) {
		if (!even_exponent & Sgl_islessthan(result,src)) 
			Sgl_increment(result);
		guardbit = Sgl_lowmantissa(result);
		Sgl_rightshiftby1(result);

		/*  now round result  */
		switch (Rounding_mode()) {
		case ROUNDPLUS:
		     Sgl_increment(result);
		     break;
		case ROUNDNEAREST:
		     /* stickybit is always true, so guardbit 
		      * is enough to determine rounding */
		     if (guardbit) {
			Sgl_increment(result);
		     }
		     break;
		}
		/* increment result exponent by 1 if mantissa overflowed */
		if (Sgl_isone_hiddenoverflow(result)) src_exponent+=2;

		if (Is_inexacttrap_enabled()) {
			Sgl_set_exponent(result,
			 ((src_exponent-SGL_BIAS)>>1)+SGL_BIAS);
			*dstptr = result;
			return(INEXACTEXCEPTION);
		}
		else Set_inexactflag();
	}
	else {
		Sgl_rightshiftby1(result);
	}
	Sgl_set_exponent(result,((src_exponent-SGL_BIAS)>>1)+SGL_BIAS);
	*dstptr = result;
	return(NOEXCEPTION);
}

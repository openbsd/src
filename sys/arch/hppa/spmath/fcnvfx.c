/*	$OpenBSD: fcnvfx.c,v 1.3 1998/07/02 19:05:19 mickey Exp $	*/

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
#include "../spmath/dbl_float.h"
#include "../spmath/cnv_float.h"

/*
 *  Single Floating-point to Single Fixed-point 
 */
/*ARGSUSED*/
int
sgl_to_sgl_fcnvfx(srcptr,nullptr,dstptr,status)

sgl_floating_point *srcptr, *nullptr, *status;
int *dstptr;
{
	register unsigned int src, temp;
	register int src_exponent, result;
	register boolean inexact = FALSE;

	src = *srcptr;
	src_exponent = Sgl_exponent(src) - SGL_BIAS;

	/* 
	 * Test for overflow
	 */
	if (src_exponent > SGL_FX_MAX_EXP) {
		/* check for MININT */
		if ((src_exponent > SGL_FX_MAX_EXP + 1) || 
		Sgl_isnotzero_mantissa(src) || Sgl_iszero_sign(src)) {
			/* 
		 	 * Since source is a number which cannot be 
			 * represented in fixed-point format, return
			 * largest (or smallest) fixed-point number.
		 	 */
			Sgl_return_overflow(src,dstptr);
		}
	}
	/*
	 * Generate result
	 */
	if (src_exponent >= 0) {
		temp = src;
		Sgl_clear_signexponent_set_hidden(temp);
		Int_from_sgl_mantissa(temp,src_exponent);
		if (Sgl_isone_sign(src))  result = -Sgl_all(temp);
		else result = Sgl_all(temp);

		/* check for inexact */
		if (Sgl_isinexact_to_fix(src,src_exponent)) {
			inexact = TRUE;
			/*  round result  */
			switch (Rounding_mode()) {
			case ROUNDPLUS:
			     if (Sgl_iszero_sign(src)) result++;
			     break;
			case ROUNDMINUS:
			     if (Sgl_isone_sign(src)) result--;
			     break;
			case ROUNDNEAREST:
			     if (Sgl_isone_roundbit(src,src_exponent)) {
			        if (Sgl_isone_stickybit(src,src_exponent) 
				|| (Sgl_isone_lowmantissa(temp))) {
			           if (Sgl_iszero_sign(src)) result++;
			           else result--;
				}
			     }
			} 
		}
	}
	else {
		result = 0;

		/* check for inexact */
		if (Sgl_isnotzero_exponentmantissa(src)) {
			inexact = TRUE;
			/*  round result  */
			switch (Rounding_mode()) {
			case ROUNDPLUS:
			     if (Sgl_iszero_sign(src)) result++;
			     break;
			case ROUNDMINUS:
			     if (Sgl_isone_sign(src)) result--;
			     break;
			case ROUNDNEAREST:
			     if (src_exponent == -1)
			        if (Sgl_isnotzero_mantissa(src)) {
			           if (Sgl_iszero_sign(src)) result++;
			           else result--;
				}
			} 
		}
	}
	*dstptr = result;
	if (inexact) {
		if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
		else Set_inexactflag();
	}
	return(NOEXCEPTION);
}

/*
 *  Single Floating-point to Double Fixed-point 
 */
/*ARGSUSED*/
int
sgl_to_dbl_fcnvfx(srcptr,nullptr,dstptr,status)

sgl_floating_point *srcptr;
dbl_integer *dstptr;
void *nullptr;
unsigned int *status;
{
	register int src_exponent, resultp1;
	register unsigned int src, temp, resultp2;
	register boolean inexact = FALSE;

	src = *srcptr;
	src_exponent = Sgl_exponent(src) - SGL_BIAS;

	/* 
	 * Test for overflow
	 */
	if (src_exponent > DBL_FX_MAX_EXP) {
		/* check for MININT */
		if ((src_exponent > DBL_FX_MAX_EXP + 1) || 
		Sgl_isnotzero_mantissa(src) || Sgl_iszero_sign(src)) {
			/* 
		 	 * Since source is a number which cannot be 
			 * represented in fixed-point format, return
			 * largest (or smallest) fixed-point number.
		 	 */
			Sgl_return_overflow_dbl(src,dstptr);
		}
		Dint_set_minint(resultp1,resultp2);
		Dint_copytoptr(resultp1,resultp2,dstptr);
		return(NOEXCEPTION);
	}
	/*
	 * Generate result
	 */
	if (src_exponent >= 0) {
		temp = src;
		Sgl_clear_signexponent_set_hidden(temp);
		Dint_from_sgl_mantissa(temp,src_exponent,resultp1,resultp2);
		if (Sgl_isone_sign(src)) {
			Dint_setone_sign(resultp1,resultp2);
		}

		/* check for inexact */
		if (Sgl_isinexact_to_fix(src,src_exponent)) {
			inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Sgl_iszero_sign(src)) {
				Dint_increment(resultp1,resultp2);
			     }
                             break;
                        case ROUNDMINUS:
                             if (Sgl_isone_sign(src)) {
				Dint_decrement(resultp1,resultp2);
			     }
                             break;
                        case ROUNDNEAREST:
                             if (Sgl_isone_roundbit(src,src_exponent))
                                if (Sgl_isone_stickybit(src,src_exponent) || 
				(Dint_isone_lowp2(resultp2))) {
				   if (Sgl_iszero_sign(src)) {
				      Dint_increment(resultp1,resultp2);
				   }
                                   else {
				      Dint_decrement(resultp1,resultp2);
				   }
				}
                        }
                }
        }
	else {
		Dint_setzero(resultp1,resultp2);

		/* check for inexact */
		if (Sgl_isnotzero_exponentmantissa(src)) {
			inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Sgl_iszero_sign(src)) {
				Dint_increment(resultp1,resultp2);
			     }
                             break;
                        case ROUNDMINUS:
                             if (Sgl_isone_sign(src)) {
				Dint_decrement(resultp1,resultp2);
			     }
                             break;
                        case ROUNDNEAREST:
                             if (src_exponent == -1)
                                if (Sgl_isnotzero_mantissa(src)) {
                                   if (Sgl_iszero_sign(src)) {
				      Dint_increment(resultp1,resultp2);
				   }
                                   else {
				      Dint_decrement(resultp1,resultp2);
				   }
				}
			}
		}
	}
	Dint_copytoptr(resultp1,resultp2,dstptr);
	if (inexact) {
		if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
		else Set_inexactflag();
	}
	return(NOEXCEPTION);
}

/*
 *  Double Floating-point to Single Fixed-point 
 */
/*ARGSUSED*/
int
dbl_to_sgl_fcnvfx(srcptr,nullptr,dstptr,status)

dbl_floating_point *srcptr;
int *dstptr;
void *nullptr;
unsigned int *status;
{
	register unsigned int srcp1,srcp2, tempp1,tempp2;
	register int src_exponent, result;
	register boolean inexact = FALSE;

	Dbl_copyfromptr(srcptr,srcp1,srcp2);
	src_exponent = Dbl_exponent(srcp1) - DBL_BIAS;

	/* 
	 * Test for overflow
	 */
	if (src_exponent > SGL_FX_MAX_EXP) {
		/* check for MININT */
		if (Dbl_isoverflow_to_int(src_exponent,srcp1,srcp2)) {
			/* 
			 * Since source is a number which cannot be 
			 * represented in fixed-point format, return
			 * largest (or smallest) fixed-point number.
			 */
			Dbl_return_overflow(srcp1,srcp2,dstptr);
		}
	}
	/*
	 * Generate result
	 */
	if (src_exponent >= 0) {
		tempp1 = srcp1;
		tempp2 = srcp2;
		Dbl_clear_signexponent_set_hidden(tempp1);
		Int_from_dbl_mantissa(tempp1,tempp2,src_exponent);
		if (Dbl_isone_sign(srcp1) && (src_exponent <= SGL_FX_MAX_EXP))
			result = -Dbl_allp1(tempp1);
		else result = Dbl_allp1(tempp1);

		/* check for inexact */
		if (Dbl_isinexact_to_fix(srcp1,srcp2,src_exponent)) {
                        inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Dbl_iszero_sign(srcp1)) result++;
                             break;
                        case ROUNDMINUS:
                             if (Dbl_isone_sign(srcp1)) result--;
                             break;
                        case ROUNDNEAREST:
                             if (Dbl_isone_roundbit(srcp1,srcp2,src_exponent))
                                if (Dbl_isone_stickybit(srcp1,srcp2,src_exponent) || 
				(Dbl_isone_lowmantissap1(tempp1))) {
                                   if (Dbl_iszero_sign(srcp1)) result++;
                                   else result--;
				}
                        } 
			/* check for overflow */
			if ((Dbl_iszero_sign(srcp1) && result < 0) ||
			    (Dbl_isone_sign(srcp1) && result > 0)) {
				Dbl_return_overflow(srcp1,srcp2,dstptr);
			}
                }
	}
	else {
		result = 0;

		/* check for inexact */
		if (Dbl_isnotzero_exponentmantissa(srcp1,srcp2)) {
                        inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Dbl_iszero_sign(srcp1)) result++;
                             break;
                        case ROUNDMINUS:
                             if (Dbl_isone_sign(srcp1)) result--;
                             break;
                        case ROUNDNEAREST:
                             if (src_exponent == -1)
                                if (Dbl_isnotzero_mantissa(srcp1,srcp2)) {
                                   if (Dbl_iszero_sign(srcp1)) result++;
                                   else result--;
				}
			}
                }
	}
	*dstptr = result;
        if (inexact) {
                if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
		else Set_inexactflag();
        }
	return(NOEXCEPTION);
}

/*
 *  Double Floating-point to Double Fixed-point 
 */
/*ARGSUSED*/
int
dbl_to_dbl_fcnvfx(srcptr,nullptr,dstptr,status)

dbl_floating_point *srcptr;
dbl_integer *dstptr;
void *nullptr;
unsigned int *status;
{
	register int src_exponent, resultp1;
	register unsigned int srcp1, srcp2, tempp1, tempp2, resultp2;
	register boolean inexact = FALSE;

	Dbl_copyfromptr(srcptr,srcp1,srcp2);
	src_exponent = Dbl_exponent(srcp1) - DBL_BIAS;

	/* 
	 * Test for overflow
	 */
	if (src_exponent > DBL_FX_MAX_EXP) {
		/* check for MININT */
		if ((src_exponent > DBL_FX_MAX_EXP + 1) || 
		Dbl_isnotzero_mantissa(srcp1,srcp2) || Dbl_iszero_sign(srcp1)) {
			/* 
		 	 * Since source is a number which cannot be 
			 * represented in fixed-point format, return
			 * largest (or smallest) fixed-point number.
		 	 */
			Dbl_return_overflow_dbl(srcp1,srcp2,dstptr);
		}
	}
 
	/*
	 * Generate result
	 */
	if (src_exponent >= 0) {
		tempp1 = srcp1;
		tempp2 = srcp2;
		Dbl_clear_signexponent_set_hidden(tempp1);
		Dint_from_dbl_mantissa(tempp1,tempp2,src_exponent,
				       resultp1, resultp2);
		if (Dbl_isone_sign(srcp1)) {
			Dint_setone_sign(resultp1,resultp2);
		}

		/* check for inexact */
		if (Dbl_isinexact_to_fix(srcp1,srcp2,src_exponent)) {
                        inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Dbl_iszero_sign(srcp1)) {
				Dint_increment(resultp1,resultp2);
			     }
                             break;
                        case ROUNDMINUS:
                             if (Dbl_isone_sign(srcp1)) {
				Dint_decrement(resultp1,resultp2);
			     }
                             break;
                        case ROUNDNEAREST:
                             if (Dbl_isone_roundbit(srcp1,srcp2,src_exponent))
                                if (Dbl_isone_stickybit(srcp1,srcp2,src_exponent) || 
				(Dint_isone_lowp2(resultp2))) {
                                   if (Dbl_iszero_sign(srcp1)) {
				      Dint_increment(resultp1,resultp2);
				   }
                                   else {
				      Dint_decrement(resultp1,resultp2);
				   }
				}
                        } 
                }
	}
	else {
		Dint_setzero(resultp1,resultp2);

		/* check for inexact */
		if (Dbl_isnotzero_exponentmantissa(srcp1,srcp2)) {
                        inexact = TRUE;
                        /*  round result  */
                        switch (Rounding_mode()) {
                        case ROUNDPLUS:
                             if (Dbl_iszero_sign(srcp1)) {
				Dint_increment(resultp1,resultp2);
			     }
                             break;
                        case ROUNDMINUS:
                             if (Dbl_isone_sign(srcp1)) {
				Dint_decrement(resultp1,resultp2);
			     }
                             break;
                        case ROUNDNEAREST:
                             if (src_exponent == -1)
                                if (Dbl_isnotzero_mantissa(srcp1,srcp2)) {
                                   if (Dbl_iszero_sign(srcp1)) {
				      Dint_increment(resultp1,resultp2);
				   }
                                   else {
				      Dint_decrement(resultp1,resultp2);
				   } 
				}
			}
                }
	}
	Dint_copytoptr(resultp1,resultp2,dstptr);
        if (inexact) {
                if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
        	else Set_inexactflag();
        }
	return(NOEXCEPTION);
}

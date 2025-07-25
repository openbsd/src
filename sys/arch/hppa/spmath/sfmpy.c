/*	$OpenBSD: sfmpy.c,v 1.6 2025/06/28 13:24:21 miod Exp $	*/
/*
  (c) Copyright 1986 HEWLETT-PACKARD COMPANY
  To anyone who acknowledges that this file is provided "AS IS"
  without any express or implied warranty:
      permission to use, copy, modify, and distribute this file
  for any purpose is hereby granted without fee, provided that
  the above copyright notice and this notice appears in all
  copies, and that the name of Hewlett-Packard Company not be
  used in advertising or publicity pertaining to distribution
  of the software without specific, written prior permission.
  Hewlett-Packard Company makes no representations about the
  suitability of this software for any purpose.
*/
/* @(#)sfmpy.c: Revision: 2.9.88.1 Date: 93/12/07 15:07:07 */

#include "float.h"
#include "sgl_float.h"

/*
 *  Single Precision Floating-point Multiply
 */
int
sgl_fmpy(sgl_floating_point *srcptr1, sgl_floating_point *srcptr2,
    sgl_floating_point *dstptr, unsigned int *status)
{
	register unsigned int opnd1, opnd2, opnd3, result;
	register int dest_exponent, count;
	register int inexact = FALSE, guardbit = FALSE, stickybit = FALSE;
	int is_tiny;

	opnd1 = *srcptr1;
	opnd2 = *srcptr2;
	/*
	 * set sign bit of result
	 */
	if (Sgl_sign(opnd1) ^ Sgl_sign(opnd2)) Sgl_setnegativezero(result);
	else Sgl_setzero(result);
	/*
	 * check first operand for NaN's or infinity
	 */
	if (Sgl_isinfinity_exponent(opnd1)) {
		if (Sgl_iszero_mantissa(opnd1)) {
			if (Sgl_isnotnan(opnd2)) {
				if (Sgl_iszero_exponentmantissa(opnd2)) {
					/*
					 * invalid since operands are infinity
					 * and zero
					 */
					if (Is_invalidtrap_enabled())
						return(INVALIDEXCEPTION);
					Set_invalidflag();
					Sgl_makequietnan(result);
					*dstptr = result;
					return(NOEXCEPTION);
				}
				/*
				 * return infinity
				 */
				Sgl_setinfinity_exponentmantissa(result);
				*dstptr = result;
				return(NOEXCEPTION);
			}
		}
		else {
			/*
			 * is NaN; signaling or quiet?
			 */
			if (Sgl_isone_signaling(opnd1)) {
				/* trap if INVALIDTRAP enabled */
				if (Is_invalidtrap_enabled())
					return(INVALIDEXCEPTION);
				/* make NaN quiet */
				Set_invalidflag();
				Sgl_set_quiet(opnd1);
			}
			/*
			 * is second operand a signaling NaN?
			 */
			else if (Sgl_is_signalingnan(opnd2)) {
				/* trap if INVALIDTRAP enabled */
				if (Is_invalidtrap_enabled())
					return(INVALIDEXCEPTION);
				/* make NaN quiet */
				Set_invalidflag();
				Sgl_set_quiet(opnd2);
				*dstptr = opnd2;
				return(NOEXCEPTION);
			}
			/*
			 * return quiet NaN
			 */
			*dstptr = opnd1;
			return(NOEXCEPTION);
		}
	}
	/*
	 * check second operand for NaN's or infinity
	 */
	if (Sgl_isinfinity_exponent(opnd2)) {
		if (Sgl_iszero_mantissa(opnd2)) {
			if (Sgl_iszero_exponentmantissa(opnd1)) {
				/* invalid since operands are zero & infinity */
				if (Is_invalidtrap_enabled())
					return(INVALIDEXCEPTION);
				Set_invalidflag();
				Sgl_makequietnan(opnd2);
				*dstptr = opnd2;
				return(NOEXCEPTION);
			}
			/*
			 * return infinity
			 */
			Sgl_setinfinity_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
		/*
		 * is NaN; signaling or quiet?
		 */
		if (Sgl_isone_signaling(opnd2)) {
			/* trap if INVALIDTRAP enabled */
			if (Is_invalidtrap_enabled()) return(INVALIDEXCEPTION);

			/* make NaN quiet */
			Set_invalidflag();
			Sgl_set_quiet(opnd2);
		}
		/*
		 * return quiet NaN
		 */
		*dstptr = opnd2;
		return(NOEXCEPTION);
	}
	/*
	 * Generate exponent
	 */
	dest_exponent = Sgl_exponent(opnd1) + Sgl_exponent(opnd2) - SGL_BIAS;

	/*
	 * Generate mantissa
	 */
	if (Sgl_isnotzero_exponent(opnd1)) {
		/* set hidden bit */
		Sgl_clear_signexponent_set_hidden(opnd1);
	}
	else {
		/* check for zero */
		if (Sgl_iszero_mantissa(opnd1)) {
			Sgl_setzero_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
		/* is denormalized, adjust exponent */
		Sgl_clear_signexponent(opnd1);
		Sgl_leftshiftby1(opnd1);
		Sgl_normalize(opnd1,dest_exponent);
	}
	/* opnd2 needs to have hidden bit set with msb in hidden bit */
	if (Sgl_isnotzero_exponent(opnd2)) {
		Sgl_clear_signexponent_set_hidden(opnd2);
	}
	else {
		/* check for zero */
		if (Sgl_iszero_mantissa(opnd2)) {
			Sgl_setzero_exponentmantissa(result);
			*dstptr = result;
			return(NOEXCEPTION);
		}
		/* is denormalized; want to normalize */
		Sgl_clear_signexponent(opnd2);
		Sgl_leftshiftby1(opnd2);
		Sgl_normalize(opnd2,dest_exponent);
	}

	/* Multiply two source mantissas together */

	Sgl_leftshiftby4(opnd2);     /* make room for guard bits */
	Sgl_setzero(opnd3);
	/*
	 * Four bits at a time are inspected in each loop, and a
	 * simple shift and add multiply algorithm is used.
	 */
	for (count=1;count<SGL_P;count+=4) {
		stickybit |= Slow4(opnd3);
		Sgl_rightshiftby4(opnd3);
		if (Sbit28(opnd1)) Sall(opnd3) += (Sall(opnd2) << 3);
		if (Sbit29(opnd1)) Sall(opnd3) += (Sall(opnd2) << 2);
		if (Sbit30(opnd1)) Sall(opnd3) += (Sall(opnd2) << 1);
		if (Sbit31(opnd1)) Sall(opnd3) += Sall(opnd2);
		Sgl_rightshiftby4(opnd1);
	}
	/* make sure result is left-justified */
	if (Sgl_iszero_sign(opnd3)) {
		Sgl_leftshiftby1(opnd3);
	}
	else {
		/* result mantissa >= 2. */
		dest_exponent++;
	}
	/* check for denormalized result */
	while (Sgl_iszero_sign(opnd3)) {
		Sgl_leftshiftby1(opnd3);
		dest_exponent--;
	}
	/*
	 * check for guard, sticky and inexact bits
	 */
	stickybit |= Sgl_all(opnd3) << (SGL_BITLENGTH - SGL_EXP_LENGTH + 1);
	guardbit = Sbit24(opnd3);
	inexact = guardbit | stickybit;

	/* re-align mantissa */
	Sgl_rightshiftby8(opnd3);

	/*
	 * round result
	 */
	if (inexact && (dest_exponent>0 || Is_underflowtrap_enabled())) {
		Sgl_clear_signexponent(opnd3);
		switch (Rounding_mode()) {
			case ROUNDPLUS:
				if (Sgl_iszero_sign(result))
					Sgl_increment(opnd3);
				break;
			case ROUNDMINUS:
				if (Sgl_isone_sign(result))
					Sgl_increment(opnd3);
				break;
			case ROUNDNEAREST:
				if (guardbit &&
				    (stickybit || Sgl_isone_lowmantissa(opnd3)))
					Sgl_increment(opnd3);
				break;
		}
		if (Sgl_isone_hidden(opnd3)) dest_exponent++;
	}
	Sgl_set_mantissa(result,opnd3);

	/*
	 * Test for overflow
	 */
	if (dest_exponent >= SGL_INFINITY_EXPONENT) {
		/* trap if OVERFLOWTRAP enabled */
		if (Is_overflowtrap_enabled()) {
			/*
			 * Adjust bias of result
			 */
			Sgl_setwrapped_exponent(result,dest_exponent,ovfl);
			*dstptr = result;
			if (inexact) {
			    if (Is_inexacttrap_enabled())
				return(OVERFLOWEXCEPTION | INEXACTEXCEPTION);
			    else Set_inexactflag();
			}
			return(OVERFLOWEXCEPTION);
		}
		inexact = TRUE;
		Set_overflowflag();
		/* set result to infinity or largest number */
		Sgl_setoverflow(result);
	}
	/*
	 * Test for underflow
	 */
	else if (dest_exponent <= 0) {
		/* trap if UNDERFLOWTRAP enabled */
		if (Is_underflowtrap_enabled()) {
			/*
			 * Adjust bias of result
			 */
			Sgl_setwrapped_exponent(result,dest_exponent,unfl);
			*dstptr = result;
			if (inexact) {
			    if (Is_inexacttrap_enabled())
				return(UNDERFLOWEXCEPTION | INEXACTEXCEPTION);
			    else Set_inexactflag();
			}
			return(UNDERFLOWEXCEPTION);
		}

		/* Determine if should set underflow flag */
		is_tiny = TRUE;
		if (dest_exponent == 0 && inexact) {
			switch (Rounding_mode()) {
			case ROUNDPLUS:
				if (Sgl_iszero_sign(result)) {
					Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
						is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			case ROUNDMINUS:
				if (Sgl_isone_sign(result)) {
					Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
						is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			case ROUNDNEAREST:
				if (guardbit && (stickybit ||
				    Sgl_isone_lowmantissa(opnd3))) {
					Sgl_increment(opnd3);
					if (Sgl_isone_hiddenoverflow(opnd3))
						is_tiny = FALSE;
					Sgl_decrement(opnd3);
				}
				break;
			}
		}

		/*
		 * denormalize result or set to signed zero
		 */
		stickybit = inexact;
		Sgl_denormalize(opnd3,dest_exponent,guardbit,stickybit,inexact);

		/* return zero or smallest number */
		if (inexact) {
			switch (Rounding_mode()) {
			case ROUNDPLUS:
				if (Sgl_iszero_sign(result))
					Sgl_increment(opnd3);
				break;
			case ROUNDMINUS:
				if (Sgl_isone_sign(result))
					Sgl_increment(opnd3);
				break;
			case ROUNDNEAREST:
				if (guardbit && (stickybit ||
				    Sgl_isone_lowmantissa(opnd3)))
					Sgl_increment(opnd3);
				break;
			}
		if (is_tiny) Set_underflowflag();
		}
		Sgl_set_exponentmantissa(result,opnd3);
	}
	else Sgl_set_exponent(result,dest_exponent);
	*dstptr = result;

	/* check for inexact */
	if (inexact) {
		if (Is_inexacttrap_enabled()) return(INEXACTEXCEPTION);
		else Set_inexactflag();
	}
	return(NOEXCEPTION);
}

/*	$OpenBSD: fpgetsticky.c,v 1.1 1996/04/21 23:38:41 deraadt Exp $	*/

/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <ieeefp.h>
#include <machine/cpufunc.h>
#include <machine/fpu.h>

fp_except
fpgetsticky()
{
	fp_except x;
	fp_except ebits = FPC_IEN | FPC_OVE | FPC_IVE | FPC_DZE | FPC_UNDE;

	sfsr(x);
	/* Map FPC_UF to soft underflow enable */
	if (x & FPC_UF)
		x |= FPC_UNDE << 1;
	else
		x &= ~(FPC_UNDE << 1);
	x >>= 1;

	return x & ebits;
}

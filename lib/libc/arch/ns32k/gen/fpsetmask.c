/*	$OpenBSD: fpsetmask.c,v 1.1 1996/04/21 23:38:44 deraadt Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <ieeefp.h>
#include <machine/cpufunc.h>
#include <machine/fpu.h>

fp_except
fpsetmask(mask)
	fp_except mask;
{
	fp_except old;
	fp_except new;
	fp_except ebits = FPC_IEN | FPC_OVE | FPC_IVE | FPC_DZE | FPC_UNDE;

	sfsr(old);

	new = old;
	new &= ~ebits;
	new |= mask & ebits;

	lfsr(new);

	return old & ebits;
}

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpsetmask.c,v 1.2 1996/08/19 08:16:43 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

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

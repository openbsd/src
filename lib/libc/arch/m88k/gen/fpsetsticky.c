/*	$OpenBSD: fpsetsticky.c,v 1.2 2003/01/07 22:01:29 miod Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to m88k by Nivas Madhur.
 */

#include <ieeefp.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpsetsticky.c,v 1.2 2003/01/07 22:01:29 miod Exp $";
#endif /* LIBC_SCCS and not lint */

fp_except
fpsetsticky(sticky)
	fp_except sticky;
{
	fp_except old;
	fp_except new;

	__asm__ volatile("fldcr %0,fcr62" : "=r" (old));

	new = old;
	new &= ~(0x1f); 
	new |= (sticky & 0x1f);

	__asm__ volatile("fstcr %0,fcr62" : : "r" (new));

	return (old & 0x1f);
}

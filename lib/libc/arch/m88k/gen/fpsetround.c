/*	$OpenBSD: fpsetround.c,v 1.2 2003/01/07 22:01:29 miod Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to 88k by Nivas Madhur
 */

#include <ieeefp.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpsetround.c,v 1.2 2003/01/07 22:01:29 miod Exp $";
#endif /* LIBC_SCCS and not lint */

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	fp_rnd old;
	fp_rnd new;

	__asm__ volatile("fldcr %0,fcr63" : "=r" (old));

	new = old;
	new &= ~(0x03 << 14); 		/* clear old value */
	new |= ((rnd_dir & 0x03) << 14);/* and set new one */

	__asm__ volatile("fstcr %0,fcr63" : : "r" (new));

	return (old >> 14) & 0x03;
}

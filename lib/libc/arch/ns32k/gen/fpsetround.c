/*
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpsetround.c,v 1.2 1996/08/19 08:16:45 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_rnd
fpsetround(rnd_dir)
	fp_rnd rnd_dir;
{
	fp_rnd old;
	fp_rnd new;

	__asm__("sfsr %0" : "=r" (old));

	new = old;
	new &= ~(0x03 << 7); 
	new |= ((rnd_dir & 0x03) << 7);

	__asm__("lfsr %0" : : "r" (new));

	return (old >> 7) & 0x03;
}

/*
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpgetround.c,v 1.2 1996/08/19 08:16:42 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_rnd
fpgetround()
{
	int x;

	__asm__("sfsr %0" : "=r" (x));
	return (x >> 7) & 0x03;
}

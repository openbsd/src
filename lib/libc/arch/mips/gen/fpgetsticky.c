/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpgetsticky.c,v 1.2 1996/08/19 08:15:56 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_except
fpgetsticky()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	return (x >> 2) & 0x1f;
}

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: fpgetsticky.c,v 1.2 1996/08/19 08:17:31 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <ieeefp.h>

fp_except
fpgetsticky()
{
	int x;

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return (x >> 5) & 0x1f;
}

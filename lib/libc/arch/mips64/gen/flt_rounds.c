/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: flt_rounds.c,v 1.1 2004/08/11 17:30:59 pefo Exp $";
#endif /* LIBC_SCCS and not lint */

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	__asm__("nop");
	return map[x & 0x03];
}

/*	$OpenBSD: flt_rounds.c,v 1.2 2003/01/07 21:59:49 miod Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: flt_rounds.c,v 1.2 2003/01/07 21:59:49 miod Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * Ported to 88k (Nivas Madhur)
 */

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	3,	/* round to negative infinity */
	2	/* round to positive infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("fldcr %0,fcr63" : "=r" (x));
	return map[(x >> 14) & 0x03];
}

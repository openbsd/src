/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

/*
 * Ported to 88k (Nivas Madhur)
 */

static const int map[] = {
	0,	/* round to nearest */
	1,	/* round to zero */
	2,	/* round to negative infinity */
	3	/* round to positive infinity */
};

int
__flt_rounds()
{
	int x;

	__asm__("fldcr %0,fcr63" : "=r" (x));
	return map[(x >> 14) & 0x03];
}

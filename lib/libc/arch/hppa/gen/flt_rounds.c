/*	$OpenBSD: flt_rounds.c,v 1.4 2012/06/25 17:01:10 deraadt Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain.
 */

#include <sys/types.h>
#include <float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	u_int64_t fpsr;

	__asm__ __volatile__("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return map[(fpsr >> 41) & 0x03];
}

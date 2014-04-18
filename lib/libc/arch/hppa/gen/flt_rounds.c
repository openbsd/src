/*	$OpenBSD: flt_rounds.c,v 1.5 2014/04/18 15:09:52 guenther Exp $	*/

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

	__asm__ volatile("fstd %%fr0,0(%1)" : "=m" (fpsr) : "r" (&fpsr));
	return map[(fpsr >> 41) & 0x03];
}

/*	$OpenBSD: flt_rounds.c,v 1.2 2002/05/22 20:05:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

static const int map[] = {
	1,	/* round to nearest */
	0,	/* round to zero */
	2,	/* round to positive infinity */
	3	/* round to negative infinity */
};

int
__flt_rounds()
{
	u_int32_t fpsr;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	return map[(fpsr >> 9) & 0x03];
}

/*	$OpenBSD: flt_rounds.c,v 1.4 2005/08/07 16:40:15 espie Exp $ */
/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <sys/types.h>
#include <machine/float.h>

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

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return map[(x >> 30) & 0x03];
}

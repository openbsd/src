/*	$OpenBSD: flt_rounds.c,v 1.2 1997/08/01 21:36:32 deraadt Exp $	*/

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
	double tmp;
	int x;

	asm("mffs %0; stfiwx %0,0,%1" : "=f"(tmp): "b"(&x));
	return map[x & 0x03];
}

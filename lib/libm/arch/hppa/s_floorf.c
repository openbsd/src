/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_floorf.c,v 1.1 2002/05/22 21:34:56 mickey Exp $";
#endif

#include <sys/types.h>
#include <machine/ieeefp.h>
#include "math.h"

float
__ieee754_floorf(float x)
{
	u_int32_t ofpsr, fpsr;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m" (ofpsr) : "r" (&ofpsr));
	fpsr = (ofpsr & ~0x600) | (FP_RN << 9);
	__asm__ __volatile__("fldw 0(%0), %%fr0" :: "r" (&fpsr));

	__asm__ __volatile__("frnd,sgl %0,%0" : "+f" (x));

	__asm__ __volatile__("fldw 0(%0), %%fr0" :: "r" (&ofpsr));
	return (x);
}

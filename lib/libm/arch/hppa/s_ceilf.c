/*
 * Written by Michael Shalayeff. Public Domain
 */

#if defined(LIBM_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: s_ceilf.c,v 1.3 2003/01/16 19:17:33 mickey Exp $";
#endif

#include <sys/types.h>
#include <machine/ieeefp.h>
#include "math.h"

float
ceilf(float x)
{
	u_int64_t ofpsr, fpsr;

	__asm__ __volatile__("fstds %%fr0,0(%1)" : "=m" (ofpsr) : "r" (&ofpsr));
	fpsr = (ofpsr & ~((u_int64_t)FP_RM << (9 + 32))) |
	    ((u_int64_t)FP_RP << (9 + 32));
	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&fpsr));

	__asm__ __volatile__("frnd,sgl %0,%0" : "+f" (x));

	__asm__ __volatile__("fldds 0(%0), %%fr0" :: "r" (&ofpsr));
	return (x);
}

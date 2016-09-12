/*	$OpenBSD: s_trunc.c,v 1.8 2016/09/12 19:47:02 guenther Exp $	*/
/*
 * Written by Michael Shalayeff. Public Domain
 */

#include <sys/types.h>
#include <machine/ieeefp.h>
#include "math.h"

double
trunc(double x)
{
	u_int64_t ofpsr, fpsr;

	__asm__ volatile("fstds %%fr0,0(%0)" :: "r" (&ofpsr) : "memory");
	fpsr = (ofpsr & ~((u_int64_t)FP_RM << (9 + 32))) |
	    ((u_int64_t)FP_RZ << (9 + 32));
	__asm__ volatile("fldds 0(%0), %%fr0" :: "r" (&fpsr) : "memory");

	__asm__ volatile("frnd,dbl %0,%0" : "+f" (x));

	__asm__ volatile("fldds 0(%0), %%fr0" :: "r" (&ofpsr) : "memory");
	return (x);
}
DEF_STD(trunc);
LDBL_UNUSED_CLONE(trunc);

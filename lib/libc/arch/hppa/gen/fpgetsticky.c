/*	$OpenBSD: fpgetsticky.c,v 1.2 2002/05/22 20:05:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

#include <sys/types.h>
#include <ieeefp.h>

fp_except
fpgetsticky()
{
	u_int32_t fpsr;

	__asm__ __volatile__("fstw %%fr0,0(%1)" : "=m"(fpsr) : "r"(&fpsr));
	return ((fpsr >> 27) & 0x1f);
}

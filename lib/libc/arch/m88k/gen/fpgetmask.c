/*	$OpenBSD: fpgetmask.c,v 1.3 2005/08/07 16:40:14 espie Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to 88k by Nivas Madhur.
 */

#include <ieeefp.h>

fp_except
fpgetmask()
{
	int x;

	__asm__ volatile ("fldcr %0,fcr63" : "=r" (x));
	return (x & 0x1f);
}

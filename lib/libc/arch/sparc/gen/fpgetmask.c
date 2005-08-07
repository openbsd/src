/*	$OpenBSD: fpgetmask.c,v 1.3 2005/08/07 16:40:15 espie Exp $ */
/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <ieeefp.h>

fp_except
fpgetmask()
{
	int x;

	__asm__("st %%fsr,%0" : "=m" (*&x));
	return (x >> 23) & 0x1f;
}

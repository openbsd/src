/*	$OpenBSD: fpsetsticky.c,v 1.3 2005/08/07 16:40:15 espie Exp $ */
/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 */

#include <ieeefp.h>

fp_except
fpsetsticky(sticky)
	fp_except sticky;
{
	fp_except old;
	fp_except new;

	__asm__("st %%fsr,%0" : "=m" (*&old));

	new = old;
	new &= ~(0x1f << 5); 
	new |= ((sticky & 0x1f) << 5);

	__asm__("ld %0,%%fsr" : : "m" (*&new));

	return (old >> 5) & 0x1f;
}

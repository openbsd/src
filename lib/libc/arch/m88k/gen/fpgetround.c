/*	$OpenBSD: fpgetround.c,v 1.4 2013/01/05 11:20:55 miod Exp $	*/

/*
 * Written by J.T. Conklin, Apr 10, 1995
 * Public domain.
 * Ported to 88k by Nivas Madhur.
 */

#include <ieeefp.h>

fp_rnd
fpgetround()
{
	int x;

	__asm__ volatile ("fldcr %0, %%fcr63" : "=r" (x));
	return (x >> 14) & 0x03;
}

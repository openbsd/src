/*
 * Written by J.T. Conklin, Apr 11, 1995
 * Public domain.
 */

#include <ieeefp.h>

fp_rnd
fpgetround()
{
	int x;

	__asm__("cfc1 %0,$31" : "=r" (x));
	return x & 0x03;
}

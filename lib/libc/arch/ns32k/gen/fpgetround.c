/*
 * Written by J.T. Conklin, Apr 28, 1995
 * Public domain.
 */

#include <ieeefp.h>

fp_rnd
fpgetround()
{
	int x;

	__asm__("sfsr %0" : "=r" (x));
	return (x >> 7) & 0x03;
}

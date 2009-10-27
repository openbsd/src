/*
 * Written by Michael Shalayeff. Public Domain
 */

#include "math.h"

float
sqrtf(float x)
{
	__asm__ __volatile__ ("fsqrt,sgl %0, %0" : "+f" (x));
	return (x);
}

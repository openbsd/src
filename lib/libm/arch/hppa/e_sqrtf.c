/*
 * Written by Michael Shalayeff. Public Domain
 */

#include "math.h"

float
sqrtf(float x)
{
	__asm__ volatile ("fsqrt,sgl %0, %0" : "+f" (x));
	return (x);
}

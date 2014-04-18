/*
 * Written by Michael Shalayeff. Public Domain
 */

#include "math.h"

float
rintf(float x)
{
	__asm__ volatile("frnd,sgl %0,%0" : "+f" (x));

	return (x);
}

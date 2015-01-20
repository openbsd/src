/*	$OpenBSD: s_rintf.c,v 1.5 2015/01/20 04:41:01 krw Exp $	*/
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

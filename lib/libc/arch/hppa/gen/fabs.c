/*	$OpenBSD: fabs.c,v 1.2 2002/05/22 20:05:01 miod Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f"(val));
	return (val);
}

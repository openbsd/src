/*	$OpenBSD: fabs.c,v 1.3 2011/07/08 22:28:33 martynas Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

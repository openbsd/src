/*	$OpenBSD: fabs.c,v 1.1 2005/04/01 10:54:27 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

/*	$OpenBSD: fabs.c,v 1.3 2002/10/21 18:41:05 mickey Exp $	*/

/*
 * Written by Miodrag Vallat.  Public domain
 */

double
fabs(double val)
{

	__asm__ __volatile__("fabs,dbl %0,%0" : "+f" (val));
	return (val);
}

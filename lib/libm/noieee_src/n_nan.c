/*	$OpenBSD: n_nan.c,v 1.1 2009/03/28 16:16:30 martynas Exp $	*/

/*
 * Written by Martynas Venckus.  Public domain
 */

#include <math.h>

/* ARGSUSED */
double
nan(const char *tagp)
{
	return (0);
}

/* ARGSUSED */
float
nanf(const char *tagp)
{
	return (0);
}

/* ARGSUSED */
long double
nanl(const char *tagp)
{
	return (0);
}


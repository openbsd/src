/*	$OpenBSD: s_signbitl_e.c,v 1.1 2007/06/01 05:53:27 jason Exp $	*/

/*
 * Written by Jason L. Wright (jason@thought.net) in 2007 and placed
 * into the public domain.
 */

#include "math.h"
#include "math_private.h"

/* __signbitl for extended long double */

int
__signbitl(long double l)
{
	ieee_extended_shape_type e;
	e.value = l;
	return (e.parts.msw & 0x80000000);
}

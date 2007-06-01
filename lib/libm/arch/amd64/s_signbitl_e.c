/*	$OpenBSD: s_signbitl_e.c,v 1.1 2007/06/01 05:56:50 jason Exp $	*/

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
	ieee_quad_shape_type e;
	e.value = l;
	return (e.parts.mswlo & 0x8000);
}

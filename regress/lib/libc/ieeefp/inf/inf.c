/*	$OpenBSD: inf.c,v 1.2 2002/02/18 11:24:13 art Exp $	*/

/*
 * Peter Valchev <pvalchev@openbsd.org> Public Domain, 2002.
 */

#include <math.h>

int
main()
{
	if (isinf(HUGE_VAL))
		return 0;

	return 1;
}

/*	$OpenBSD: inf.c,v 1.1 2002/02/16 17:22:16 pvalchev Exp $	*/

/*
 * Peter Valchev <pvalchev@openbsd.org> Public Domain, 2002.
 */

#include <math.h>

int
main() {
	if (isinf(HUGE_VAL))
		return 0;
}

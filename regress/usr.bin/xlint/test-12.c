/*	$OpenBSD: test-12.c,v 1.2 2005/12/10 19:20:21 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint warnings regarding assignment in conditional context.
 */
#include <limits.h>

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	int a = 0;

	if (a = argc)		/* should warn */
		return 1;

	a++;

	if ((a = argc))		/* should not warn */
		return 1;

	for (a = 0; a < 10; a++)
		a = a;

	return 0;
}



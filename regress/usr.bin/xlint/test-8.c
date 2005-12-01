/*      $OpenBSD: test-8.c,v 1.1 2005/12/01 05:07:55 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test warning on things declared in the translation unit but never
 * defined.
 */
#include "test-8.h"

int	foo(int);	/* warning: declared but never used or defined */

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	return 0;
}

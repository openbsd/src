/*	$OpenBSD: test-10.c,v 1.2 2005/12/09 03:34:34 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint warning about literal char assignments.
 */
#include <limits.h>

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	unsigned char c;

	c = '\377';	/* should not warn, because c is a char type */
	c = -1;		/* should warn, because rvalue is not a char literal */
	c++;
	
	return 0;
}



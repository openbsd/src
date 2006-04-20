 /*	$OpenBSD: test-18.c,v 1.1 2006/04/20 04:03:05 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test lint dealing with LINTUSED comments.
 */

/* LINTUSED */
int g;

int u;

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	/* LINTUSED */
	int a, b;
	int c;

	return 0;
}

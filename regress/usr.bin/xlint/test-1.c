/*      $OpenBSD: test-1.c,v 1.2 2005/11/21 19:37:11 cloder Exp $ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test the ARGSUSED feature of lint.
 */

int
unusedargs(int unused)
{
	return 0;
}

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	unusedargs(1);
	return 0;
}

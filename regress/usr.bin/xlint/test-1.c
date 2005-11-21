/*      $OpenBSD: test-1.c,v 1.1 2005/11/21 18:25:57 cloder Exp $ */

/*
 * This program is in the public domain.
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

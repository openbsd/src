/*      $OpenBSD: test-7.c,v 1.1 2005/11/30 19:44:08 cloder Exp $	*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test slashslash comments.
 */

int	foo(int);	// comment at end of line, ok
//comment at beginning of line

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	return 0;
}

/*	$OpenBSD: test-26.c,v 1.1 2011/05/31 22:35:19 martynas Exp $	*/

/*
 * Placed in the public domain by Martynas Venckus <martynas@openbsd.org>.
 *
 * Test lint warnings about empty non-compound selection statements.
 */

/* ARGSUSED */
int
main(void)
{
	if (0);
	if (0) 0;
	if (0) {}

	if (0) {} else;
	if (0) {} else 0;
	if (0) {} else {}

	return (0);
}

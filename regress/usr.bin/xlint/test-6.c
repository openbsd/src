/*      $OpenBSD: test-6.c,v 1.1 2005/11/30 19:39:03 cloder Exp $ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test c99 predifined identifier __func__
 */
#include <string.h>

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	/* c99 implicitly defines: static const char __func__[] = "main"; */
	static const char foo[] = "main";
	char c;

	if (strcmp(foo, __func__) == 0)
		return 1;

	__func__[0] = 'a';	/* warning: const (not an lvalue) */
	c = __func__[4];	/* ok (c == '\0') */
	c = __func__[5];	/* warning: out of bonds */

	c++;
	return 0;
}

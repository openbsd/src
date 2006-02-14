/*      $OpenBSD: test-5.c,v 1.2 2006/02/14 16:11:45 moritz Exp $ */

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Test warning of promotion of function arguments.
 */

#include <stdint.h>

void
foo(unsigned long long a)
{
	a++;
}

void foobar(int a)
{
	a++;
}

void bar(unsigned int a)
{
	a++;
}

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	int a = 0;

	foo(0);		   /* ok, promotion of in-range constant 	  */
	foo(a);		   /* warning: promotion of non-constant 	  */
	foobar(INTMAX_MAX);/* warning: promotion of out-of-range constant */
	bar(-1);	   /* warning: promotion of out-of-range constant */
	bar(0);		   /* ok, promotion of in-range constant 	  */
	return 0;
}

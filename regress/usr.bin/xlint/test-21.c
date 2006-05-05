 /*	$OpenBSD: test-21.c,v 1.2 2006/05/05 06:48:20 otto Exp $*/

/*
 * Placed in the public domain by Chad Loder <cloder@openbsd.org>.
 *
 * Regression test lint1 crash on function prototypes having functions
 * as arguments.
 */
struct foo
{
	int a;
};

int bar(int, unsigned int(int, const struct foo *, int));
int baz(int *(void *));

/* ARGSUSED */
int
main(int argc, char* argv[])
{
	return 0;
}

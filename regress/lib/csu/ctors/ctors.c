/*	$OpenBSD: ctors.c,v 1.1 2002/01/31 15:33:36 art Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org>. Public Domain.
 */
void foo(void) __attribute__((constructor));

int constructed = 0;

void
foo(void)
{
	constructed = 1;
}

int
main()
{
	return !constructed;
}

/*	$OpenBSD: ctors.c,v 1.2 2002/02/18 11:03:58 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
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

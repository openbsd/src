/*	$OpenBSD: alloca.c,v 1.3 2003/07/31 03:23:41 mickey Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public Domain.	*/

#include <stdio.h>

int
main()
{
	char *q, *p;

	p = alloca(41);
	strcpy(p, "hellow world");

	q = alloca(53);
	strcpy(q, "hellow world");

	exit(strcmp(p, q));
}

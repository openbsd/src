/*	$OpenBSD: alloca.c,v 1.4 2003/07/31 21:48:02 deraadt Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public Domain.	*/

#include <stdio.h>

int
main(int argc, char *argv[])
{
	char *q, *p;

	p = alloca(41);
	strcpy(p, "hellow world");

	q = alloca(53);
	strcpy(q, "hellow world");

	exit(strcmp(p, q));
}

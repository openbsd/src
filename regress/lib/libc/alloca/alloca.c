/*	$OpenBSD: alloca.c,v 1.2 2003/06/26 17:15:27 david Exp $	*/

/*	Copyright (c) 2003 Michael Shalayeff. Public Domain.	*/

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

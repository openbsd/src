/*	$OpenBSD: alloca.c,v 1.5 2003/08/02 01:24:36 david Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public Domain.	*/

#include <stdio.h>
#include <string.h>

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

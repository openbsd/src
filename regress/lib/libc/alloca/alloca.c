/*	$OpenBSD: alloca.c,v 1.6 2003/09/02 23:52:16 david Exp $	*/

/*	Written by Michael Shalayeff, 2003, Public Domain.	*/

#include <stdio.h>
#include <stdlib.h>
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

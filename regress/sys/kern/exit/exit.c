/*	$OpenBSD: exit.c,v 1.1 2002/02/08 20:15:14 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdlib.h>

int
main()
{	
	_exit(0);
	abort();
}

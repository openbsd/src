/*	$OpenBSD: exit.c,v 1.2 2003/07/31 21:48:08 deraadt Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdlib.h>

int
main(int argc, char *argv[])
{	
	_exit(0);
	abort();
}

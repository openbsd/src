/*
 * Public domain. 2002, Matthieu Herrb
 *
 * $OpenBSD: main.c,v 1.1 2002/02/05 21:47:23 matthieu Exp $
 */
#include <stdio.h>
#include "elfbug.h"

int (*func)(void) = uninitialized;

int
main(int argc, char *argv[])
{
	fooinit();
	return (*func)();
}


/*	$OpenBSD: Lint_rfork.c,v 1.1 2004/09/14 22:18:35 deraadt Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
rfork(int flags)
{
	return (0);
}

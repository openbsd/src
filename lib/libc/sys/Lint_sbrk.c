/*	$OpenBSD: Lint_sbrk.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_sbrk.c,v 1.1 1997/11/06 00:53:04 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
char *
sbrk(int incr)
{
	return (0);
}

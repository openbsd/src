/*	$OpenBSD: Lint_brk.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_brk.c,v 1.1 1997/11/06 00:52:52 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
char *
brk(const char *addr)
{
	return (0);
}

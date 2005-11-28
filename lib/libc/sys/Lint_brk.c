/*	$OpenBSD: Lint_brk.c,v 1.3 2005/11/28 16:54:07 deraadt Exp $	*/
/*	$NetBSD: Lint_brk.c,v 1.1 1997/11/06 00:52:52 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
void *
brk(void *addr)
{
	return (0);
}

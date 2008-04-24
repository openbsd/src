/*	$OpenBSD: Lint_brk.c,v 1.4 2008/04/24 20:43:20 kurt Exp $	*/
/*	$NetBSD: Lint_brk.c,v 1.1 1997/11/06 00:52:52 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
brk(void *addr)
{
	return (0);
}

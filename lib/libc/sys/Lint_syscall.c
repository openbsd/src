/*	$OpenBSD: Lint_syscall.c,v 1.2 2002/02/19 19:39:37 millert Exp $	*/
/*	$NetBSD: Lint_syscall.c,v 1.1 1997/11/06 00:53:22 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>
#include <stdarg.h>

/*ARGSUSED*/
int
syscall(int arg1, ...)
{
	return (0);
}

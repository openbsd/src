/*	$OpenBSD: Lint_exect.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_exect.c,v 1.1 1997/11/06 00:52:54 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
exect(const char *path, char * const *argv, char * const *envp)
{
	return (0);
}

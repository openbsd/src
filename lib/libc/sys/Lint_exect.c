/*	$OpenBSD: Lint_exect.c,v 1.1 1998/02/08 22:45:09 tholo Exp $	*/
/*	$NetBSD: Lint_exect.c,v 1.1 1997/11/06 00:52:54 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
exect(path, argv, envp)
	const char *path;
	char * const * argv;
	char * const * envp;
{
	return (0);
}

/*	$OpenBSD: Lint_pipe.c,v 1.1 1998/02/08 22:45:10 tholo Exp $	*/
/*	$NetBSD: Lint_pipe.c,v 1.1 1997/11/06 00:52:59 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
pipe(filedes)
	int filedes[2];
{
	return (0);
}

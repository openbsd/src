/*	$OpenBSD: Lint_Ovfork.c,v 1.2 2004/09/14 22:18:56 deraadt Exp $	*/
/*	$NetBSD: Lint_Ovfork.c,v 1.1 1997/11/06 00:52:49 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
pid_t
vfork(void)
{
	return (0);
}

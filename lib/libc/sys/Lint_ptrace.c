/*	$OpenBSD: Lint_ptrace.c,v 1.1 1998/02/08 22:45:10 tholo Exp $	*/
/*	$NetBSD: Lint_ptrace.c,v 1.1 1997/11/06 00:53:02 cgd Exp $	*/

/*
 * This file placed in the public domain.
 * Chris Demetriou, November 5, 1997.
 */

#include <unistd.h>

/*ARGSUSED*/
int
ptrace(request, pid, addr, data)
	int request;
	pid_t pid;
	caddr_t addr;
	int data;
{
	return (0);
}

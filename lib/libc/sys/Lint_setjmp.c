/*	$OpenBSD: Lint_setjmp.c,v 1.1 2007/09/09 19:22:45 otto Exp $	*/

/* Public domain, Otto Moerbeek, 2007 */

#include <setjmp.h>

/*ARGSUSED*/
int
setjmp(jmp_buf env)
{
	return 0;
}

/*ARGSUSED*/
int
_setjmp(jmp_buf env)
{
	return 0;
}

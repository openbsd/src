/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: getdtablesize.c,v 1.3 1995/05/11 23:03:44 jtc Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>

int
getdtablesize()
{
	return sysconf(_SC_OPEN_MAX);
}

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: getdtablesize.c,v 1.3 2003/06/11 21:03:10 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>

int
getdtablesize(void)
{
	return sysconf(_SC_OPEN_MAX);
}

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: getdtablesize.c,v 1.2 1996/08/19 08:19:20 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <unistd.h>

int
getdtablesize()
{
	return sysconf(_SC_OPEN_MAX);
}

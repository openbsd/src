#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: errno.c,v 1.3 2004/07/13 21:03:35 marc Exp $";
#endif /* LIBC_SCCS and not lint */

#include <errno.h>
#undef errno

/*
 * global errno for unthreaded programs.
 */
int errno;

/*
 * weak version of function used by unthreaded programs.
 */
int *
___errno(void)
{
	return &errno;
}

__weak_alias(__errno, ___errno);

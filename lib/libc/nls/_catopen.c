/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _catopen.c,v 1.5 2005/03/23 21:13:28 otto Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_catopen,catopen);
#else

#include <nl_types.h>

extern nl_catd _catopen(const char *, int);

nl_catd
catopen(const char *name, int oflag)
{
	return _catopen(name, oflag);
}

#endif

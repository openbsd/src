/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _catclose.c,v 1.4 2005/03/23 21:13:28 otto Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_catclose,catclose);
#else

#include <nl_types.h>

extern int _catclose(nl_catd);

int
catclose(nl_catd catd)
{
	return _catclose(catd);
}

#endif

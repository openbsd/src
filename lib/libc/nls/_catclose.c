/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _catclose.c,v 1.3 2002/02/16 21:27:23 millert Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_catclose,catclose);
#else

#include <nl_types.h>

extern int _catclose(nl_catd);

int
catclose(catd)
	nl_catd catd;
{
	return _catclose(catd);
}

#endif

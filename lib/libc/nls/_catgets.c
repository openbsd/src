/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _catgets.c,v 1.4 2005/03/23 21:13:28 otto Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_catgets,catgets);
#else

#include <nl_types.h>

extern char * _catgets(nl_catd, int, int, const char *);

char *
catgets(nl_catd catd, int set_id, int msg_id, const char *s)
{
	return _catgets(catd, set_id, msg_id, s);
}

#endif

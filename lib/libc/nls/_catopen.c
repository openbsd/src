/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _catopen.c,v 1.3 1996/09/15 09:31:21 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_catopen,catopen);
#else

#include <nl_types.h>

extern nl_catd _catopen __P((const char *, int));

nl_catd
catopen(name, oflag)
	const char *name;
	int oflag;
{
	return _catopen(name, oflag);
}

#endif

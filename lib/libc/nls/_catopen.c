/*	$OpenBSD: _catopen.c,v 1.6 2005/08/05 13:03:00 espie Exp $ */
/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

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

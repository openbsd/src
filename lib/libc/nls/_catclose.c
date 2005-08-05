/*	$OpenBSD: _catclose.c,v 1.5 2005/08/05 13:03:00 espie Exp $ */
/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

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

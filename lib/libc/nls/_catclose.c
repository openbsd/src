/*	$OpenBSD: _catclose.c,v 1.6 2012/12/05 23:20:00 deraadt Exp $ */
/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/types.h>

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

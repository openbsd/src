/*	$OpenBSD: _catgets.c,v 1.6 2012/12/05 23:20:00 deraadt Exp $ */
/*
 * Written by J.T. Conklin, 10/05/94
 * Public domain.
 */

#include <sys/types.h>

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

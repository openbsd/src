/*	$OpenBSD: errno.c,v 1.5 2005/08/08 08:05:34 espie Exp $ */
/* PUBLIC DOMAIN: No Rights Reserved.   Marco S Hyman <marc@snafu.org> */

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

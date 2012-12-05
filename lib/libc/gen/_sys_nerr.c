/*	$OpenBSD: _sys_nerr.c,v 1.4 2012/12/05 23:19:59 deraadt Exp $ */
/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/types.h>

#ifdef __indr_reference
__indr_reference(_sys_nerr, sys_nerr);
__indr_reference(_sys_nerr, __sys_nerr); /* Backwards compat with v.12 */
#endif

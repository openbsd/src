/*	$OpenBSD: _sys_nerr.c,v 1.3 2005/08/08 08:05:33 espie Exp $ */
/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_sys_nerr, sys_nerr);
__indr_reference(_sys_nerr, __sys_nerr); /* Backwards compat with v.12 */
#endif

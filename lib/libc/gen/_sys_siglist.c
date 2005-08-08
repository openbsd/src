/*	$OpenBSD: _sys_siglist.c,v 1.3 2005/08/08 08:05:33 espie Exp $ */
/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_sys_siglist, sys_siglist);
__indr_reference(_sys_siglist, __sys_siglist); /* Backwards compat with v.12 */
#else

#undef _sys_siglist
#define	_sys_siglist	sys_siglist
#include "siglist.c"

#undef _sys_siglist
#define	_sys_siglist	__sys_siglist
#include "siglist.c"

#endif

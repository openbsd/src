/*	$OpenBSD: _verr.c,v 1.2 1996/04/21 23:39:07 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_verr, verr);
#else

#define _verr	verr
#define rcsid	_rcsid
#include "verr.c"

#endif

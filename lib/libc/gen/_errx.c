/*	$OpenBSD: _errx.c,v 1.2 1996/04/21 23:39:05 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_errx, errx);
#else

#define _errx    errx
#define rcsid   _rcsid
#include "errx.c"

#endif

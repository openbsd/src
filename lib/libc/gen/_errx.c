/*	$OpenBSD: _errx.c,v 1.3 1996/05/01 12:56:19 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_errx, errx);
#else

#define _errx    errx
#define _verrx   verrx
#define rcsid   _rcsid
#include "errx.c"

#endif

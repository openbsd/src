/*	$OpenBSD: _vwarnx.c,v 1.2 1996/04/21 23:39:12 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_vwarnx, vwarnx);
#else

#define _vwarnx	vwarnx
#define rcsid   _rcsid
#include "vwarnx.c"

#endif

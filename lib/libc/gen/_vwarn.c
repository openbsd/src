/*	$OpenBSD: _vwarn.c,v 1.2 1996/04/21 23:39:10 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_vwarn, vwarn);
#else

#define _vwarn	vwarn
#define rcsid   _rcsid
#include "vwarn.c"

#endif

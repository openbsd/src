/*	$OpenBSD: _warn.c,v 1.2 1996/04/21 23:39:14 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_warn, warn);
#else

#define _warn  warn
#define rcsid   _rcsid
#include "warn.c"

#endif

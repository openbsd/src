/*	$OpenBSD: _err.c,v 1.3 1996/05/01 12:56:18 deraadt Exp $	*/

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_err, err);
#else

#define	_err	err
#define	_verr	verr
#define	rcsid	_rcsid
#include "err.c"

#endif

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_err, err);
#else

#define	_err	err
#define _errx	errx
#define	_warn	warn
#define	_warnx	warnx
#define	_verr	verr
#define _verrx	verrx
#define	_vwarn	vwarn
#define	_vwarnx	vwarnx
#define	rcsid	_rcsid
#include "err.c"

#endif

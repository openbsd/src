/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_sys_errlist, sys_errlist);
__indr_reference(_sys_errlist, __sys_errlist); /* Backwards compat with v.12 */
#else

#undef _sys_errlist
#undef _sys_nerr
#undef rcsid
#define	_sys_errlist	sys_errlist
#define	_sys_nerr	sys_nerr
#define	rcsid		_rcsid
#include "errlist.c"

#undef _sys_errlist
#undef _sys_nerr
#undef rcsid
#define	_sys_errlist	__sys_errlist
#define	_sys_nerr	__sys_nerr
#define	rcsid		__rcsid
#include "errlist.c"

#endif

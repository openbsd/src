/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _sys_siglist.c,v 1.2 1996/08/19 08:21:27 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_sys_siglist, sys_siglist);
__indr_reference(_sys_siglist, __sys_siglist); /* Backwards compat with v.12 */
#else

#undef _sys_siglist
#undef rcsid
#define	_sys_siglist	sys_siglist
#define	rcsid		_rcsid
#include "siglist.c"

#undef _sys_siglist
#undef rcsid
#define	_sys_siglist	__sys_siglist
#define	rcsid		__rcsid
#include "siglist.c"

#endif

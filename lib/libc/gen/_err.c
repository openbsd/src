/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _err.c,v 1.4 1996/08/19 08:21:19 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_err, err);
#else

#define	_err	err
#define	_verr	verr
#define	rcsid	_rcsid
#include "err.c"

#endif

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _errx.c,v 1.4 1996/08/19 08:21:21 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_errx, errx);
#else

#define _errx    errx
#define _verrx   verrx
#define rcsid   _rcsid
#include "errx.c"

#endif

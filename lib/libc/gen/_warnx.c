/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _warnx.c,v 1.3 1996/08/19 08:21:38 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_warnx, warnx);
#else

#define _warnx  warnx
#define rcsid   _rcsid
#include "warnx.c"

#endif

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _vwarnx.c,v 1.3 1996/08/19 08:21:35 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_vwarnx, vwarnx);
#else

#define _vwarnx	vwarnx
#define rcsid   _rcsid
#include "vwarnx.c"

#endif

/*
 * J.T. Conklin, December 12, 1994
 * Public Domain
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _vwarn.c,v 1.3 1996/08/19 08:21:33 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_vwarn, vwarn);
#else

#define _vwarn	vwarn
#define rcsid   _rcsid
#include "vwarn.c"

#endif

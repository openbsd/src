/*
 * Written by J.T. Conklin, December 12, 1994
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: _sys_nerr.c,v 1.2 1996/08/19 08:21:25 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/cdefs.h>

#ifdef __indr_reference
__indr_reference(_sys_nerr, sys_nerr);
__indr_reference(_sys_nerr, __sys_nerr); /* Backwards compat with v.12 */
#endif

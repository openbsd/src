/*	$OpenBSD: infinity.c,v 1.1 1999/09/14 00:21:15 mickey Exp $	*/

/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.1 1999/09/14 00:21:15 mickey Exp $";
#endif /* LIBC_SCCS and not lint */

#include <math.h>

/* bytes for +Infinity on a hppa */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

/*	$OpenBSD: infinity.c,v 1.1 2005/04/01 10:54:27 mickey Exp $	*/

/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.1 2005/04/01 10:54:27 mickey Exp $";
#endif /* LIBC_SCCS and not lint */

#include <math.h>

/* bytes for +Infinity on a hppa */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.2 2004/02/08 17:31:44 deraadt Exp $";
#endif /* LIBC_SCCS and not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
	{ 0, 0, 0, 0, 0, 0, (char)0xf0, 0x7f };


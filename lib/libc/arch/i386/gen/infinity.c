#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.2 1996/08/19 08:12:30 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };

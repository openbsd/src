/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.2 1996/08/19 08:17:37 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <math.h>

/* bytes for +Infinity on a sparc */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

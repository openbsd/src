/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.3 1996/08/19 08:16:47 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <math.h>

/* bytes for +Infinity on a ns32k */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };

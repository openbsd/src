/* infinity.c */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.3 1996/11/29 16:51:02 imp Exp $";
#endif /* LIBC_SCCS and not lint */

#include <math.h>
#include <sys/types.h>

/* bytes for +Infinity on a MIPS */
#if BYTE_ORDER == BIG_ENDIAN
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#else
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif

/*	$OpenBSD: infinity.c,v 1.2 2005/08/07 16:40:15 espie Exp $ */
/* infinity.c */

#include <math.h>
#include <sys/types.h>

/* bytes for +Infinity on a MIPS */
#if BYTE_ORDER == BIG_ENDIAN
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#else
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif

/*	$OpenBSD: infinity.c,v 1.2 2006/11/12 21:18:28 otto Exp $	*/

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a SH4 FPU (double precision) */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
#if _BYTE_ORDER == _LITTLE_ENDIAN
    { 0, 0, 0, 0, 0, 0, (char)0xf0, 0x7f };
#else
    { 0x7f, (char)0xf0, 0, 0, 0, 0, 0, 0 }
#endif

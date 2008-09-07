/*	$OpenBSD: nan.c,v 1.2 2008/09/07 20:36:07 martynas Exp $	*/

/* Written by Martynas Venckus.  Public Domain. */

#include <sys/types.h>
#include <math.h>
#include <machine/endian.h>

/* bytes for qNaN on a mips64 (IEEE single format) */
char __nan[] __attribute__((__aligned__(sizeof(float)))) =
#if BYTE_ORDER == BIG_ENDIAN
					{ 0x7f, 0xc0, 0, 0 };
#else /* BYTE_ORDER == BIG_ENDIAN */
					{ 0, 0, 0xc0, 0x7f };
#endif /* BYTE_ORDER == BIG_ENDIAN */

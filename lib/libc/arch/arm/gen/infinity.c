/*	$OpenBSD: infinity.c,v 1.3 2004/02/02 07:03:21 drahn Exp $	*/
/*	$NetBSD: infinity.c,v 1.3 2002/02/19 20:08:19 bjh21 Exp $	*/

/*
 * IEEE-compatible infinity.c -- public domain.
 */

#include <sys/types.h>
#include <math.h>
#include <machine/endian.h>

char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
#if BYTE_ORDER == BIG_ENDIAN
	{ 0x7f, 0xf0,    0,    0, 0, 0,    0,    0};
#else
#ifdef __VFP_FP__
	{    0,    0,    0,    0, 0, 0, 0xf0, 0x7f};
#else
	{    0,    0, 0xf0, 0x7f, 0, 0,    0,    0};
#endif
#endif

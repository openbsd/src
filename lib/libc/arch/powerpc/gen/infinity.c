/*	$OpenBSD: infinity.c,v 1.5 2005/08/07 16:40:15 espie Exp $	*/

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a PowerPC */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

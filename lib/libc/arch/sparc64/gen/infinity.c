/*	$OpenBSD: infinity.c,v 1.1 2001/08/29 01:41:29 art Exp $	*/

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a sparc */
char __infinity[] = { 0x7f, (char)0xf0, 0, 0, 0, 0, 0, 0 };

/*	$OpenBSD: infinity.c,v 1.3 2005/08/07 16:40:15 espie Exp $ */
/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a sparc */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

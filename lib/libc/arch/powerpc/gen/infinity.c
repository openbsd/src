/*	$OpenBSD: infinity.c,v 1.4 2003/01/29 15:02:02 drahn Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: infinity.c,v 1.4 2003/01/29 15:02:02 drahn Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a PowerPC */
char __infinity[] __attribute__((__aligned__(sizeof(double)))) =
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

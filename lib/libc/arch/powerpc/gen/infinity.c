/*	$OpenBSD: infinity.c,v 1.3 2001/08/25 15:20:15 drahn Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: infinity.c,v 1.3 2001/08/25 15:20:15 drahn Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a PowerPC */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

/*	$OpenBSD: infinity.c,v 1.2 1996/04/21 23:38:49 deraadt Exp $	*/

#ifndef lint
static char rcsid[] = "$OpenBSD: infinity.c,v 1.2 1996/04/21 23:38:49 deraadt Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a ns32k */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };

#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1.1.1 1995/10/18 08:41:37 deraadt Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a ns32k */
char __infinity[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xef, 0x7f };

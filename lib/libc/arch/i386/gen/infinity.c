#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1.1.1 1995/10/18 08:41:24 deraadt Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };

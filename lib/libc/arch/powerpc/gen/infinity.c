#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1.1.1 1996/12/21 20:42:22 rahnds Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a 387 */
char __infinity[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };

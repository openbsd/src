#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1 1997/03/25 17:07:01 rahnds Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on 88100 */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1.1.1 1995/10/18 08:41:29 deraadt Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on a 68k */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

#ifndef lint
static char rcsid[] = "$Id: infinity.c,v 1.1 1998/12/15 07:10:30 smurph Exp $";
#endif /* not lint */

/* infinity.c */

#include <math.h>

/* bytes for +Infinity on 88100 */
char __infinity[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };

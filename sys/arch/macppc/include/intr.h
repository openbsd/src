/*	$OpenBSD: intr.h,v 1.4 2008/09/16 04:20:42 drahn Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_set_priority(int, int);
#endif

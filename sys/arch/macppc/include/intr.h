/*	$OpenBSD: intr.h,v 1.7 2009/06/02 21:38:09 drahn Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_set_priority(int);
#endif

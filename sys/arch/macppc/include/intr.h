/*	$OpenBSD: intr.h,v 1.5 2008/09/18 03:56:25 drahn Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_set_priority(int);
#endif

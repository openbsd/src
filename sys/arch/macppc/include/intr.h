/*	$OpenBSD: intr.h,v 1.8 2009/06/09 01:12:38 deraadt Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_set_priority(int, int);
#endif

/*	$OpenBSD: intr.h,v 1.6 2008/11/21 17:35:52 deraadt Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_set_priority(int, int);
#endif

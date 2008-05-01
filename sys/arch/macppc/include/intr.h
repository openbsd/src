/*	$OpenBSD: intr.h,v 1.3 2008/05/01 08:25:32 kettenis Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE
void softtty(void);

void openpic_send_ipi(int);
void openpic_set_priority(int, int);
#endif

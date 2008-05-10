/*	$OpenBSD: intr.h,v 1.1 2008/05/10 12:02:21 kettenis Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE

void softtty(void);

void *intr_establish(int, int, int,  int (*)(void *), void *, const char *);

#endif

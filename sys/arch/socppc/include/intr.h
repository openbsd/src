/*	$OpenBSD: intr.h,v 1.2 2014/04/01 20:42:39 mpi Exp $	*/

#include <powerpc/intr.h>

#ifndef _LOCORE

void *intr_establish(int, int, int,  int (*)(void *), void *, const char *);

#endif

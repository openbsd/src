/*	$OpenBSD: flockfile.c,v 1.8 2012/09/01 01:08:16 fgsch Exp $	*/

#include <sys/time.h>
#include <stdio.h>
#include "thread_private.h"

/*
 * Subroutine versions of the macros in <stdio.h>
 * Note that these are all no-ops because libc does not do threads.
 * Strong implementation of file locking in librthread/rthread_file.c
 */

#undef flockfile
#undef ftrylockfile
#undef funlockfile

WEAK_PROTOTYPE(flockfile);
WEAK_PROTOTYPE(ftrylockfile);
WEAK_PROTOTYPE(funlockfile);

WEAK_ALIAS(flockfile);
WEAK_ALIAS(ftrylockfile);
WEAK_ALIAS(funlockfile);

void
WEAK_NAME(flockfile)(FILE * fp)
{
}


int
WEAK_NAME(ftrylockfile)(FILE *fp)
{

	return 0;
}

void
WEAK_NAME(funlockfile)(FILE * fp)
{
}

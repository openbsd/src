/*	$OpenBSD: flockfile.c,v 1.6 2004/06/07 21:11:23 marc Exp $	*/

#include <sys/time.h>
#include <stdio.h>
#include "thread_private.h"

/*
 * Subroutine versions of the macros in <stdio.h>
 * Note that these are all no-ops because libc does not do threads.
 * Strong implementation of file locking in libc_r/uthread/uthread_file.c
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
WEAK_NAME(flockfile)(fp)
	FILE * fp;
{
}


int
WEAK_NAME(ftrylockfile)(fp)
	FILE * fp;
{

	return 0;
}

void
WEAK_NAME(funlockfile)(fp)
	FILE * fp;
{
}

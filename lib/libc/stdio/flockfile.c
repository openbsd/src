/*	$OpenBSD: flockfile.c,v 1.1 1998/11/20 11:18:48 d Exp $	*/

#include <stdio.h>
#include "thread_private.h"

#ifndef _THREAD_SAFE

/*
 * Subroutine versions of the macros in <stdio.h>
 * Note that these are all no-ops because libc does not do threads.
 */

#undef flockfile
#undef ftrylockfile
#undef funlockfile
#undef _flockfile_debug

void
flockfile(fp)
	FILE * fp;
{
}

int
ftrylockfile(fp)
	FILE * fp;
{
	return 0;
}

void
funlockfile(fp)
	FILE * fp;
{
}

void
_flockfile_debug(fp, fname, lineno)
	FILE * fp;
	const char * fname;
	int lineno;
{
}

#else /* _THREAD_SAFE */

/* Actual implementation of file locking in libc_r/uthread/uthread_file.c */

#endif /* _THREAD_SAFE */

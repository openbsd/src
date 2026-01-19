/*	$OpenBSD: flockfile.c,v 1.12 2026/01/19 23:01:00 guenther Exp $	*/

#include <stdio.h>
#include "local.h"

/*
 * These don't use the FLOCKFILE()/FUNLOCKFILE() macros because a
 * lock taken while single threaded by the functions below needs
 * to be a real lock if the process creates a thread while holding
 * the lock.
 */

void
flockfile(FILE *fp)
{
	__rcmtx_enter(&fp->_lock);
}
DEF_WEAK(flockfile);


int
ftrylockfile(FILE *fp)
{
	return __rcmtx_enter_try(&fp->_lock) ? 0 : 1;

	return 0;
}
DEF_WEAK(ftrylockfile);

void
funlockfile(FILE *fp)
{
	__rcmtx_leave(&fp->_lock);
}
DEF_WEAK(funlockfile);

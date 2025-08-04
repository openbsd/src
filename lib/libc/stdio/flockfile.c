/*	$OpenBSD: flockfile.c,v 1.10 2025/08/04 01:44:33 dlg Exp $	*/

#include <stdio.h>
#include "local.h"

void
flockfile(FILE *fp)
{
	FLOCKFILE(fp);
}
DEF_WEAK(flockfile);


int
ftrylockfile(FILE *fp)
{
	if (__isthreaded)
		return __rcmtx_enter_try(&_EXT(fp)->_lock) ? 0 : 1;

	return 0;
}
DEF_WEAK(ftrylockfile);

void
funlockfile(FILE *fp)
{
	FUNLOCKFILE(fp);
}
DEF_WEAK(funlockfile);

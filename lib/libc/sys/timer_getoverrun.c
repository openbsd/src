/*	$OpenBSD: timer_getoverrun.c,v 1.5 2005/08/08 08:05:37 espie Exp $ */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_getoverrun(timer_t timerid)
{
	errno = ENOSYS;
	return -1;
}

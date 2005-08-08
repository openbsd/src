/*	$OpenBSD: timer_gettime.c,v 1.6 2005/08/08 08:05:37 espie Exp $ */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct itimerspec;

/* ARGSUSED */
int
timer_gettime(timer_t timerid, struct itimerspec *value)
{
	errno = ENOSYS;
	return -1;
}

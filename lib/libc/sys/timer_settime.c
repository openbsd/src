/*	$OpenBSD: timer_settime.c,v 1.6 2005/08/08 08:05:38 espie Exp $ */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct itimerspec;

/* ARGSUSED */
int
timer_settime(timer_t timerid, int flags, const struct itimerspec *value,
    struct itimerspec *ovalue)
{
	errno = ENOSYS;
	return -1;
}

#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_gettime.c,v 1.3 1997/04/30 05:49:30 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_gettime(timerid, value)
	timer_t timerid;
	struct itimerspec *value;
{
	errno = ENOSYS;
	return -1;
}

#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_gettime.c,v 1.4 1998/02/07 20:50:55 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct itimerspec;

/* ARGSUSED */
int
timer_gettime(timerid, value)
	timer_t timerid;
	struct itimerspec *value;
{
	errno = ENOSYS;
	return -1;
}

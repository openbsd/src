#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_gettime.c,v 1.2 1997/04/26 08:49:34 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_gettime(timerid, value)
	timer_t timerid;
	struct itimerspec *value;
{
	errno = ENOSYS;
	return -1;
}

#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_settime.c,v 1.4 1998/02/07 20:50:55 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

struct itimerspec;

/* ARGSUSED */
int
timer_settime(timerid, flags, value, ovalue)
	timer_t timerid;
	int flags;
	const struct itimerspec *value;
	struct itimerspec *ovalue;
{
	errno = ENOSYS;
	return -1;
}

#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_getoverrun.c,v 1.3 1997/04/30 05:49:29 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_getoverrun(timerid)
	timer_t timerid;
{
	errno = ENOSYS;
	return -1;
}

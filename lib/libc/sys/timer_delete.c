#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_delete.c,v 1.4 2003/06/11 21:03:10 deraadt Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

/* ARGSUSED */
int
timer_delete(timer_t timerid)
{
	errno = ENOSYS;
	return -1;
}

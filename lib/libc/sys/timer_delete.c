#if defined(SYSLIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: timer_delete.c,v 1.2 1997/04/26 08:49:33 tholo Exp $";
#endif /* SYSLIBC_SCCS and not lint */

#include <signal.h>
#include <time.h>
#include <errno.h>

int
timer_delete(timerid)
	timer_t timerid;
{
	errno = ENOSYS;
	return -1;
}

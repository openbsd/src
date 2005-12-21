/*	$OpenBSD: ptrace.c,v 1.4 2005/12/21 01:40:23 millert Exp $	*/
/* David Leonard <d@openbsd.org>, 1999. Public domain. */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <errno.h>

int _ptrace(int, pid_t, caddr_t, int);

int
ptrace(int request, pid_t pid, caddr_t addr, int data)
{

	/* ptrace(2) is documented to clear errno on success: */
	errno = 0;
	return (_ptrace(request, pid, addr, data));
}

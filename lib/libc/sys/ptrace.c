/*	$OpenBSD: ptrace.c,v 1.1 1999/02/01 08:13:01 d Exp $	*/
/* David Leonard <d@openbsd.org>, 1999. Public domain. */

#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/errno.h>

int _ptrace __P((int, pid_t, caddr_t, int));

int
ptrace(request, pid, addr, data)
	int request;
	pid_t pid;
	caddr_t addr;
	int data;
{

	/* ptrace(2) is documented to clear errno on success: */
	errno = 0;
	return (_ptrace(request, pid, addr, data));
}

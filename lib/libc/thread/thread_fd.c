/*	$OpenBSD: thread_fd.c,v 1.1 2000/01/06 07:27:33 d Exp $	*/

#include <sys/time.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_fd_lock);
WEAK_PROTOTYPE(_thread_fd_lock_debug);
WEAK_PROTOTYPE(_thread_fd_unlock);
WEAK_PROTOTYPE(_thread_fd_unlock_debug);

WEAK_ALIAS(_thread_fd_lock);
WEAK_ALIAS(_thread_fd_lock_debug);
WEAK_ALIAS(_thread_fd_unlock);
WEAK_ALIAS(_thread_fd_unlock_debug);

int     
WEAK_NAME(_thread_fd_lock)(fd, lock_type, timeout)
	int	fd;
	int	lock_type;
	struct timespec *timeout;
{
	return 0;
}

int     
WEAK_NAME(_thread_fd_lock_debug)(fd, lock_type, timeout, fname, lineno)
	int	fd;
	int	lock_type;
	struct timespec *timeout;
	const char *fname;
	int	lineno;
{
	return 0;
}

void
WEAK_NAME(_thread_fd_unlock)(fd, lock_type)
	int	fd;
	int	lock_type;
{
}

void
WEAK_NAME(_thread_fd_unlock_debug)(fd, lock_type, fname, lineno)
	int	fd;
	int	lock_type;
	const char *fname;
	int	lineno;
{
}


/*	$OpenBSD: thread_fd.c,v 1.7 2002/11/05 22:19:55 marc Exp $	*/

#include <sys/time.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_fd_lock);
WEAK_PROTOTYPE(_thread_fd_unlock);

WEAK_ALIAS(_thread_fd_lock);
WEAK_ALIAS(_thread_fd_unlock);

int     
WEAK_NAME(_thread_fd_lock)(int fd, int lock_type, struct timespec *timeout,
			   const char *fname, int lineno)
{
	return 0;
}

void
WEAK_NAME(_thread_fd_unlock)(int fd, int lock_type, const char *fname,
			     int lineno)
{
}


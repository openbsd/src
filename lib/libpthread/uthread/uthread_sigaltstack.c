/*
 * $OpenBSD: uthread_sigaltstack.c,v 1.1 1998/11/09 03:13:20 d Exp $
 */

#include <signal.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/*
 * placeholder for sigaltstack XXX impl to be done
 */

int
sigaltstack(const struct sigaltstack *ss, struct sigaltstack *oss)
{
	errno = EINVAL;
	return (-1);
}
#endif /* _THREAD_SAFE */

/*	$OpenBSD: unithread_malloc_lock.c,v 1.1 2000/01/14 06:16:37 d Exp $	*/

#include <sys/cdefs.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_malloc_lock);
WEAK_PROTOTYPE(_thread_malloc_unlock);

WEAK_ALIAS(_thread_malloc_lock);
WEAK_ALIAS(_thread_malloc_unlock);

void
WEAK_NAME(_thread_malloc_lock)()
{
}

void
WEAK_NAME(_thread_malloc_unlock)()
{
}

/*	$OpenBSD: unithread_malloc_lock.c,v 1.2 2002/11/03 20:36:43 marc Exp $	*/

#include <sys/cdefs.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_thread_malloc_lock);
WEAK_PROTOTYPE(_thread_malloc_unlock);
WEAK_PROTOTYPE(_thread_malloc_init);

WEAK_ALIAS(_thread_malloc_lock);
WEAK_ALIAS(_thread_malloc_unlock);
WEAK_ALIAS(_thread_malloc_init);

void
WEAK_NAME(_thread_malloc_lock)(void)
{
	return;
}

void
WEAK_NAME(_thread_malloc_unlock)(void)
{
	return;
}

void
WEAK_NAME(_thread_malloc_init)(void)
{
	return;
}

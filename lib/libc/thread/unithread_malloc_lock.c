/*	$OpenBSD: unithread_malloc_lock.c,v 1.5 2004/06/07 21:11:23 marc Exp $	*/

#include <sys/time.h>
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

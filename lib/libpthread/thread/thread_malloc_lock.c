/* $OpenBSD: thread_malloc_lock.c,v 1.4 2002/11/05 22:19:55 marc Exp $ */
/* Public Domain <marc@snafu.org> */

#include <pthread.h>
#include "pthread_private.h"

static spinlock_t malloc_lock = _SPINLOCK_INITIALIZER;

void
_thread_malloc_lock()
{
	_SPINLOCK(&malloc_lock);
}

void
_thread_malloc_unlock()
{
	_SPINUNLOCK(&malloc_lock);
}

void
_thread_malloc_init()
{
}

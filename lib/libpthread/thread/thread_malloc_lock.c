/* $OpenBSD: thread_malloc_lock.c,v 1.5 2006/02/22 07:16:32 otto Exp $ */
/* Public Domain <marc@snafu.org> */

#include <pthread.h>
#include "pthread_private.h"

static spinlock_t malloc_lock = _SPINLOCK_INITIALIZER;
static spinlock_t atexit_lock = _SPINLOCK_INITIALIZER;

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

void
_thread_atexit_lock()
{
	_SPINLOCK(&atexit_lock);
}

void
_thread_atexit_unlock()
{
	_SPINUNLOCK(&atexit_lock);
}

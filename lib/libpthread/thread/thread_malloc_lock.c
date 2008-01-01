/* $OpenBSD: thread_malloc_lock.c,v 1.6 2008/01/01 00:43:39 kurt Exp $ */
/* Public Domain <marc@snafu.org> */

#include <pthread.h>
#include "pthread_private.h"

static spinlock_t malloc_lock = _SPINLOCK_INITIALIZER;
static spinlock_t atexit_lock = _SPINLOCK_INITIALIZER;
static spinlock_t arc4_lock = _SPINLOCK_INITIALIZER;

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

void
_thread_arc4_lock()
{
	_SPINLOCK(&arc4_lock);
}

void
_thread_arc4_unlock()
{
	_SPINUNLOCK(&arc4_lock);
}

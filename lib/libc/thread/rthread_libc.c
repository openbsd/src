/* $OpenBSD: rthread_libc.c,v 1.1 2017/08/15 06:13:24 guenther Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/time.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "thread_private.h"	/* in libc/include */

#include "rthread.h"
#include "rthread_cb.h"

/*
 * A thread tag is a pointer to a structure of this type.  An opaque
 * tag is used to decouple libc from the thread library.
 */
struct _thread_tag {
	pthread_mutex_t	m;	/* the tag's mutex */
	pthread_key_t	k;	/* a key for private data */
};

/*
 * local mutex to protect against tag creation races.
 */
static pthread_mutex_t	_thread_tag_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * Initialize a thread tag structure once.   This function is called
 * if the tag is null.  Allocation and initialization are controlled
 * by a mutex.   If the tag is not null when the mutex is obtained
 * the caller lost a race -- some other thread initialized the tag.
 * This function will never return NULL.
 */
static void
_thread_tag_init(void **tag)
{
	struct _thread_tag *tt;
	int result;

	result = pthread_mutex_lock(&_thread_tag_mutex);
	if (result == 0) {
		if (*tag == NULL) {
			tt = malloc(sizeof *tt);
			if (tt != NULL) {
				result = pthread_mutex_init(&tt->m, NULL);
				result |= pthread_key_create(&tt->k, free);
				*tag = tt;
			}
		}
		result |= pthread_mutex_unlock(&_thread_tag_mutex);
	}
	if (result != 0)
		_rthread_debug(1, "tag init failure");
}

/*
 * lock the mutex associated with the given tag
 */
void
_thread_tag_lock(void **tag)
{
	struct _thread_tag *tt;

	if (__isthreaded) {
		if (*tag == NULL)
			_thread_tag_init(tag);
		tt = *tag;
		if (pthread_mutex_lock(&tt->m) != 0)
			_rthread_debug(1, "tag mutex lock failure");
	}
}

/*
 * unlock the mutex associated with the given tag
 */
void
_thread_tag_unlock(void **tag)
{
	struct _thread_tag *tt;

	if (__isthreaded) {
		if (*tag == NULL)
			_thread_tag_init(tag);
		tt = *tag;
		if (pthread_mutex_unlock(&tt->m) != 0)
			_rthread_debug(1, "tag mutex unlock failure");
	}
}

/*
 * return the thread specific data for the given tag.   If there
 * is no data for this thread initialize it from 'storage'.
 * On any error return 'err'.
 */
void *
_thread_tag_storage(void **tag, void *storage, size_t sz, void *err)
{
	struct _thread_tag *tt;
	void *ret;

	if (*tag == NULL)
		_thread_tag_init(tag);
	tt = *tag;

	ret = pthread_getspecific(tt->k);
	if (ret == NULL) {
		ret = malloc(sz);
		if (ret == NULL)
			ret = err;
		else {
			if (pthread_setspecific(tt->k, ret) == 0)
				memcpy(ret, storage, sz);
			else {
				free(ret);
				ret = err;
			}
		}
	}
	return ret;
}

void
_thread_mutex_lock(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_lock(pmutex) != 0)
		_rthread_debug(1, "mutex lock failure");
}

void
_thread_mutex_unlock(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_unlock(pmutex) != 0)
		_rthread_debug(1, "mutex unlock failure");
}

void
_thread_mutex_destroy(void **mutex)
{
	pthread_mutex_t	*pmutex = (pthread_mutex_t *)mutex;

	if (pthread_mutex_destroy(pmutex) != 0)
		_rthread_debug(1, "mutex destroy failure");
}

/*
 * the malloc lock
 */
#ifndef FUTEX
#define MALLOC_LOCK_INITIALIZER(n) { \
	_SPINLOCK_UNLOCKED,	\
	TAILQ_HEAD_INITIALIZER(malloc_lock[n].lockers), \
	PTHREAD_MUTEX_DEFAULT,	\
	NULL,			\
	0,			\
	-1 }
#else
#define MALLOC_LOCK_INITIALIZER(n) { \
	_SPINLOCK_UNLOCKED,	\
	PTHREAD_MUTEX_DEFAULT,	\
	NULL,			\
	0,			\
	-1 }
#endif

static struct pthread_mutex malloc_lock[_MALLOC_MUTEXES] = {
	MALLOC_LOCK_INITIALIZER(0),
	MALLOC_LOCK_INITIALIZER(1),
	MALLOC_LOCK_INITIALIZER(2),
	MALLOC_LOCK_INITIALIZER(3)
};

static pthread_mutex_t malloc_mutex[_MALLOC_MUTEXES] = {
	&malloc_lock[0],
	&malloc_lock[1],
	&malloc_lock[2],
	&malloc_lock[3]
};

void
_thread_malloc_lock(int i)
{
	pthread_mutex_lock(&malloc_mutex[i]);
}

void
_thread_malloc_unlock(int i)
{
	pthread_mutex_unlock(&malloc_mutex[i]);
}

void
_thread_malloc_reinit(void)
{
	int i;

	for (i = 0; i < _MALLOC_MUTEXES; i++) {
		malloc_lock[i].lock = _SPINLOCK_UNLOCKED;
#ifndef FUTEX
		TAILQ_INIT(&malloc_lock[i].lockers);
#endif
		malloc_lock[i].owner = NULL;
		malloc_lock[i].count = 0;
	}
}

/*
 * atexit lock
 */
static _atomic_lock_t atexit_lock = _SPINLOCK_UNLOCKED;

void
_thread_atexit_lock(void)
{
	_spinlock(&atexit_lock);
}

void
_thread_atexit_unlock(void)
{
	_spinunlock(&atexit_lock);
}

/*
 * atfork lock
 */
static _atomic_lock_t atfork_lock = _SPINLOCK_UNLOCKED;

void
_thread_atfork_lock(void)
{
	_spinlock(&atfork_lock);
}

void
_thread_atfork_unlock(void)
{
	_spinunlock(&atfork_lock);
}

/*
 * arc4random lock
 */
static _atomic_lock_t arc4_lock = _SPINLOCK_UNLOCKED;

void
_thread_arc4_lock(void)
{
	_spinlock(&arc4_lock);
}

void
_thread_arc4_unlock(void)
{
	_spinunlock(&arc4_lock);
}

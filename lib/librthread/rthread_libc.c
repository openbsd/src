/* $OpenBSD: rthread_libc.c,v 1.2 2006/01/05 04:24:30 tedu Exp $ */
/* $snafu: libc_tag.c,v 1.4 2004/11/30 07:00:06 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#define _POSIX_THREADS

#include <sys/time.h>

#include <machine/spinlock.h>

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "thread_private.h"	/* in libc/include */

#include "rthread.h"

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
 * is no date for this thread initialize it from 'storage'.
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

/*
 * the malloc lock
 */
static _spinlock_lock_t malloc_lock = _SPINLOCK_UNLOCKED;

void
_thread_malloc_lock(void)
{
	_spinlock(&malloc_lock);
}

void
_thread_malloc_unlock(void)
{
	_spinunlock(&malloc_lock);
}

void
_thread_malloc_init(void)
{
}

/* $OpenBSD: thread_storage.c,v 1.4 2002/11/03 20:36:43 marc Exp $ */
/* Public Domain */

/*
 * libpthread's stronger functions
 */

#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "pthread_private.h"

void
_libc_private_storage_lock(mutex)
        pthread_mutex_t *mutex;
{
	if (__isthreaded && pthread_mutex_lock(mutex) != 0)
		PANIC("_libc_private_storage_lock");
}

void
_libc_private_storage_unlock(mutex)
        pthread_mutex_t *mutex;
{
	if (__isthreaded && pthread_mutex_unlock(mutex) != 0)
		PANIC("_libc_private_storage_unlock");
}

void *
_libc_private_storage(volkey, init, initsz, error)
	volatile struct _thread_private_key_struct * volkey;
	void *	init;
	size_t	initsz;
	void *	error;
{
	void *result;
	void (*cleanfn)(void *);
	struct _thread_private_key_struct * key;

	/* Use static storage while not threaded: */
	if (!__isthreaded)
		return init;

	key = (struct _thread_private_key_struct *)volkey;	/* for gcc */

	/* Create the key once: */
	if (volkey->once.state == PTHREAD_NEEDS_INIT) {
		if (pthread_mutex_lock(&key->once.mutex) != 0)
			return error;
		if (volkey->once.state == PTHREAD_NEEDS_INIT) {
			if (key->cleanfn == NULL)
				cleanfn = free;
			else
				cleanfn = key->cleanfn;
			if (pthread_key_create(&key->key, cleanfn) != 0) {
				pthread_mutex_unlock(&key->once.mutex);
				return error;
			}
			key->once.state = PTHREAD_DONE_INIT;
		}
		pthread_mutex_unlock(&key->once.mutex);
	}

	/* XXX signals may cause re-entrancy here? */

	/* Acquire this key's thread-specific storage: */
	result = pthread_getspecific(key->key);

	/* Allocate and initialise storage if unallocated: */
	if (result == NULL) {
		result = malloc(initsz);
		if (result == NULL)
			return error;
		if (pthread_setspecific(key->key, result) != 0) {
			free(result);
			return error;
		}
		memcpy(result, init, initsz);
	}

	return result;
}

/*	$OpenBSD: unithread_storage.c,v 1.1 2000/01/06 15:07:06 d Exp $	*/
/*
 * 
 */
/* unthreaded storage allocation helper functions */

#include <sys/cdefs.h>
#include <pthread.h>
#include "thread_private.h"

WEAK_PROTOTYPE(_libc_private_storage_lock);
WEAK_PROTOTYPE(_libc_private_storage_unlock);
WEAK_PROTOTYPE(_libc_private_storage);

WEAK_ALIAS(_libc_private_storage_lock);
WEAK_ALIAS(_libc_private_storage_unlock);
WEAK_ALIAS(_libc_private_storage);

void
WEAK_NAME(_libc_private_storage_lock)(mutex)
	pthread_mutex_t *mutex;
{
}

void
WEAK_NAME(_libc_private_storage_unlock)(mutex)
	pthread_mutex_t *mutex;
{
}

void *
WEAK_NAME(_libc_private_storage)(key, init, initsz, error)
	volatile struct _thread_private_key_struct * key;
	void *init;
	size_t initsz;
	void *error;
{

	return init;
}

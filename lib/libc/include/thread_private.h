/*
 *
 * Support for thread-safety in libc and libc_r common code using macros 
 * to declare thread-safe data structures.
 *
 * $OpenBSD: thread_private.h,v 1.2 1999/01/06 05:19:32 d Exp $
 */

#ifndef _THREAD_PRIVATE_H_
#define _THREAD_PRIVATE_H_

/*
 * Parts of this file are
 * Copyright (c) 1998 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * $Id: thread_private.h,v 1.2 1999/01/06 05:19:32 d Exp $
 * $OpenBSD: thread_private.h,v 1.2 1999/01/06 05:19:32 d Exp $
 */

/*
 * This global flag is non-zero when a process has created one
 * or more threads. It is used to avoid calling locking functions
 * when they are not required. In libc, this is always assumed
 * to be zero.
 */

extern volatile int	__isthreaded;

#ifdef _THREAD_SAFE

#include <pthread.h>
#include "pthread_private.h"

/*
 * File lock contention is difficult to diagnose without knowing
 * where locks were set. Allow a debug library to be built which
 * records the source file and line number of each lock call.
 */
#ifdef	_FLOCK_DEBUG
#define _FLOCKFILE(x)	_flockfile_debug(x, __FILE__, __LINE__)
#else
#define _FLOCKFILE(x)	flockfile(x)
#endif

/*
 * These macros help in making persistent storage thread-specific.
 * Libc makes extensive use of private static data structures
 * that hold state across function invocation, and these macros
 * are no-ops when _THREAD_SAFE is not defined. 
 * In a thread-safe library, the static variables are used only for
 * initialising the per-thread instances of the state variables.
 */

/*
 * Give names to the private variables used to hold per-thread
 * data structures.
 */
#ifdef __STDC__
#define __THREAD_MUTEXP_NAME(name)	_thread_mutexp_inst__ ## name
#define __THREAD_MUTEX_NAME(name)	_thread_mutex_inst__ ## name
#define __THREAD_KEY_NAME(name)		_thread_key_inst__ ## name
#else
#define __THREAD_MUTEXP_NAME(name)	_thread_mutexp_inst__/**/name
#define __THREAD_MUTEX_NAME(name)	_thread_mutex_inst__/**/name
#define __THREAD_KEY_NAME(name)		_thread_key_inst__/**/name
#endif

/*
 * Mutex declare, lock and unlock macros.
 */
#define _THREAD_PRIVATE_MUTEX(name)					\
	static struct pthread_mutex __THREAD_MUTEXP_NAME(name) =	\
		PTHREAD_MUTEX_STATIC_INITIALIZER;			\
	static pthread_mutex_t __THREAD_MUTEX_NAME(name) = 		\
		&__THREAD_MUTEXP_NAME(name);
		
#define _THREAD_PRIVATE_MUTEX_LOCK(name) 				\
	pthread_mutex_lock(&__THREAD_MUTEX_NAME(name))
		
#define _THREAD_PRIVATE_MUTEX_UNLOCK(name) 				\
	pthread_mutex_unlock(&__THREAD_MUTEX_NAME(name))

/*
 * A mutexed data structure used to hold the persistent state's key.
 */
struct _thread_private_key_struct {
	struct pthread_mutex	lockd;
	pthread_mutex_t		lock;
	int			init;
	pthread_key_t		key;
};

/*
 * Declaration of a per-thread state key.
 */
#define _THREAD_PRIVATE_KEY(name)					\
	static volatile struct _thread_private_key_struct		\
	__THREAD_KEY_NAME(name) = {					\
		PTHREAD_MUTEX_STATIC_INITIALIZER, 			\
		&__THREAD_KEY_NAME(name).lockd,				\
		0							\
	};

/*
 * Initialisation of storage space for a per-thread state variable.
 * A pointer to a per-thread *copy* of the _initv parameter is returned.
 * It calls malloc the first time and the space is automatically free'd
 * when the thread dies. If you need something a bit more complicated
 * than free() you will need to roll-your-own.
 */
#define _THREAD_PRIVATE(keyname, _initv, _errv) 			\
	({								\
		struct _thread_private_key_struct * __k = 		\
			&__THREAD_KEY_NAME(keyname);			\
		void* __p;						\
		extern void free __P((void*));				\
		extern void* malloc __P((size_t));			\
									\
		if (!__isthreaded) {					\
			/* non-threaded behaviour */			\
			__p = &(_initv);				\
			goto _ok;					\
		}							\
									\
		/* create key for first thread */			\
		pthread_mutex_lock(&__k->lock);				\
		if (__k->init == 0) {					\
			if (pthread_key_create(&__k->key, free)) {	\
				pthread_mutex_unlock(&__k->lock);	\
				goto _err;				\
			}						\
			__k->init = 1;					\
		}							\
		pthread_mutex_unlock(&__k->lock);			\
									\
		if ((__p = pthread_getspecific(__k->key)) == NULL) {	\
			/* alloc space on 1st call in this thread */	\
			if ((__p = malloc(sizeof(_initv))) == NULL) 	\
				goto _err;				\
				if (pthread_setspecific(__k->key, __p) != 0) { \
					free(__p);			\
					goto _err;			\
				}					\
			/* initialise with _initv */			\
		memcpy(__p, &_initv, sizeof(_initv));			\
		}							\
		goto _ok;						\
	_err:								\
		__p = (_errv);						\
	_ok:								\
		__p;							\
	})

/*
 * Macros for locking and unlocking FILEs. These test if the
 * process is threaded to avoid locking when not required.
 */
#define	FLOCKFILE(fp)		if (__isthreaded) _FLOCKFILE(fp)
#define	FUNLOCKFILE(fp)		if (__isthreaded) funlockfile(fp)

#else /* !_THREAD_SAFE */

/*
 * Do-nothing macros for single-threaded case.
 */
#define _FD_LOCK(f,o,p)				(0)
#define _FD_UNLOCK(f,o)				/* nothing */
#define _THREAD_PRIVATE_KEY(_key)		/* nothing */
#define _THREAD_PRIVATE(keyname, _initv, _errv)	(&(_initv))
#define _THREAD_PRIVATE_MUTEX(_name)		/* nothing */
#define _THREAD_PRIVATE_MUTEX_LOCK(_name)	/* nothing */
#define _THREAD_PRIVATE_MUTEX_UNLOCK(_name)	/* nothing */
#define	FLOCKFILE(fp)				/* nothing */
#define	FUNLOCKFILE(fp)				/* nothing */

#endif /* !_THREAD_SAFE */

#endif _THREAD_PRIVATE_H_

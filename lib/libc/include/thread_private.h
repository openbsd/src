/* $OpenBSD: thread_private.h,v 1.27 2016/05/07 19:05:22 guenther Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#ifndef _THREAD_PRIVATE_H_
#define _THREAD_PRIVATE_H_

#include <stdio.h>		/* for FILE and __isthreaded */

/*
 * The callbacks needed by libc to handle the threaded case.
 * NOTE: Bump the version when you change the struct contents!
 *
 * tc_canceled:
 *	If not NULL, what to do when canceled (otherwise _exit(0))
 *
 * tc_flockfile, tc_ftrylockfile, and tc_funlockfile:
 *	If not NULL, these implement the flockfile() family.
 *	XXX In theory, you should be able to lock a FILE before
 *	XXX loading libpthread and have that be a real lock on it,
 *	XXX but that doesn't work without the libc base version
 *	XXX tracking the recursion count.
 *
 * tc_malloc_lock and tc_malloc_unlock:
 * tc_atexit_lock and tc_atexit_unlock:
 * tc_atfork_lock and tc_atfork_unlock:
 * tc_arc4_lock and tc_arc4_unlock:
 *	The locks used by the malloc, atexit, atfork, and arc4 subsystems.
 *	These have to be ordered specially in the fork/vfork wrappers
 *	and may be implemented differently than the general mutexes
 *	in the callbacks below.
 *
 * tc_mutex_lock and tc_mutex_unlock:
 *	Lock and unlock the given mutex. If the given mutex is NULL
 *	a mutex is allocated and initialized automatically.
 *
 * tc_mutex_destroy:
 *	Destroy/deallocate the given mutex.
 *
 * tc_tag_lock and tc_tag_unlock:
 *	Lock and unlock the mutex associated with the given tag.
 *	If the given tag is NULL a tag is allocated and initialized
 *	automatically.
 *
 * tc_tag_storage:
 *	Returns a pointer to per-thread instance of data associated
 *	with the given tag.  If the given tag is NULL a tag is
 *	allocated and initialized automatically.
 *
 * tc_fork, tc_vfork:
 *	If not NULL, they are called instead of the syscall stub, so that
 *	the thread library can do necessary locking and reinitialization.
 *
 *
 * If <machine/tcb.h> doesn't define TCB_GET(), then locating the TCB in a
 * threaded process requires a syscall (__get_tcb(2)) which is too much
 * overhead for single-threaded processes.  For those archs, there are two
 * additional callbacks, though they are placed first in the struct for
 * convenience in ASM:
 *
 * tc_errnoptr:
 *	Returns the address of the thread's errno.
 *
 * tc_tcb:
 *	Returns the address of the thread's TCB.
 */

struct thread_callbacks {
	int	*(*tc_errnoptr)(void);		/* MUST BE FIRST */
	void	*(*tc_tcb)(void);
	__dead void	(*tc_canceled)(void);
	void	(*tc_flockfile)(FILE *);
	int	(*tc_ftrylockfile)(FILE *);
	void	(*tc_funlockfile)(FILE *);
	void	(*tc_malloc_lock)(void);
	void	(*tc_malloc_unlock)(void);
	void	(*tc_atexit_lock)(void);
	void	(*tc_atexit_unlock)(void);
	void	(*tc_atfork_lock)(void);
	void	(*tc_atfork_unlock)(void);
	void	(*tc_arc4_lock)(void);
	void	(*tc_arc4_unlock)(void);
	void	(*tc_mutex_lock)(void **);
	void	(*tc_mutex_unlock)(void **);
	void	(*tc_mutex_destroy)(void **);
	void	(*tc_tag_lock)(void **);
	void	(*tc_tag_unlock)(void **);
	void	*(*tc_tag_storage)(void **, void *, size_t, void *);
	__pid_t	(*tc_fork)(void);
	__pid_t	(*tc_vfork)(void);
};

__BEGIN_PUBLIC_DECLS
/*
 *  Set the callbacks used by libc
 */
void	_thread_set_callbacks(const struct thread_callbacks *_cb, size_t _len);
__END_PUBLIC_DECLS

#ifdef __LIBC__
__BEGIN_HIDDEN_DECLS
/* the current set */
extern struct thread_callbacks _thread_cb;
__END_HIDDEN_DECLS
#endif /* __LIBC__ */

/*
 * helper macro to make unique names in the thread namespace
 */
#define __THREAD_NAME(name)	__CONCAT(_thread_tagname_,name)

/*
 * Resolver code is special cased in that it uses global keys.
 */
extern void *__THREAD_NAME(_res);
extern void *__THREAD_NAME(_res_ext);
extern void *__THREAD_NAME(serv_mutex);

/*
 * Macros used in libc to access thread mutex, keys, and per thread storage.
 * _THREAD_PRIVATE_KEY and _THREAD_PRIVATE_MUTEX are different macros for
 * historical reasons.   They do the same thing, define a static variable
 * keyed by 'name' that identifies a mutex and a key to identify per thread
 * data.
 */
#define _THREAD_PRIVATE_KEY(name)					\
	static void *__THREAD_NAME(name)
#define _THREAD_PRIVATE_MUTEX(name)					\
	static void *__THREAD_NAME(name)


#ifndef __LIBC__	/* building some sort of reach around */

#define _THREAD_PRIVATE_MUTEX_LOCK(name)		do {} while (0)
#define _THREAD_PRIVATE_MUTEX_UNLOCK(name)		do {} while (0)
#define _THREAD_PRIVATE(keyname, storage, error)	&(storage)
#define _MUTEX_LOCK(mutex)				do {} while (0)
#define _MUTEX_UNLOCK(mutex)				do {} while (0)
#define _MUTEX_DESTROY(mutex)				do {} while (0)
#define _MALLOC_LOCK()					do {} while (0)
#define _MALLOC_UNLOCK()				do {} while (0)
#define _ATEXIT_LOCK()					do {} while (0)
#define _ATEXIT_UNLOCK()				do {} while (0)
#define _ATFORK_LOCK()					do {} while (0)
#define _ATFORK_UNLOCK()				do {} while (0)
#define _ARC4_LOCK()					do {} while (0)
#define _ARC4_UNLOCK()					do {} while (0)

#else		/* building libc */
#define _THREAD_PRIVATE_MUTEX_LOCK(name)				\
	do {								\
		if (_thread_cb.tc_tag_lock != NULL)			\
			_thread_cb.tc_tag_lock(&(__THREAD_NAME(name)));	\
	} while (0)
#define _THREAD_PRIVATE_MUTEX_UNLOCK(name)				\
	do {								\
		if (_thread_cb.tc_tag_unlock != NULL)			\
			_thread_cb.tc_tag_unlock(&(__THREAD_NAME(name))); \
	} while (0)
#define _THREAD_PRIVATE(keyname, storage, error)			\
	(_thread_cb.tc_tag_storage == NULL ? &(storage) :		\
	    _thread_cb.tc_tag_storage(&(__THREAD_NAME(keyname)),	\
		&(storage), sizeof(storage), error))

/*
 * Macros used in libc to access mutexes.
 */
#define _MUTEX_LOCK(mutex)						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_mutex_lock(mutex);		\
	} while (0)
#define _MUTEX_UNLOCK(mutex)						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_mutex_unlock(mutex);		\
	} while (0)
#define _MUTEX_DESTROY(mutex)						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_mutex_destroy(mutex);		\
	} while (0)

/*
 * malloc lock/unlock prototypes and definitions
 */
#define _MALLOC_LOCK()							\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_malloc_lock();			\
	} while (0)
#define _MALLOC_UNLOCK()						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_malloc_unlock();			\
	} while (0)

#define _ATEXIT_LOCK()							\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_atexit_lock();			\
	} while (0)
#define _ATEXIT_UNLOCK()						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_atexit_unlock();			\
	} while (0)

#define _ATFORK_LOCK()							\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_atfork_lock();			\
	} while (0)
#define _ATFORK_UNLOCK()						\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_atfork_unlock();			\
	} while (0)

#define _ARC4_LOCK()							\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_arc4_lock();			\
	} while (0)
#define _ARC4_UNLOCK()							\
	do {								\
		if (__isthreaded)					\
			_thread_cb.tc_arc4_unlock();			\
	} while (0)
#endif /* __LIBC__ */

#endif /* _THREAD_PRIVATE_H_ */

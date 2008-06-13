/* $OpenBSD: rthread_libc.c,v 1.7 2008/06/13 21:18:43 otto Exp $ */
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

/*
 * atexit lock
 */
static _spinlock_lock_t atexit_lock = _SPINLOCK_UNLOCKED;

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
 * arc4random lock
 */
static _spinlock_lock_t arc4_lock = _SPINLOCK_UNLOCKED;

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

#if 0
/*
 * miscellaneous libc exported symbols we want to override
 */
int
close(int fd)
{
	int rv;

	pthread_testcancel();
	rv = _thread_sys_close(fd);
	pthread_testcancel();
	return (rv);
}

#if 0
/* libc calls open */
int
creat(const char *path, mode_t mode)
{

}
#endif

#if 0
int
fcntl(int fd, int cmd, ...)
{
	va_list ap;
	int rv;

	pthread_testcancel();
	va_start(ap, cmd);
	rv = _thread_sys_fcntl(fd, cmd, va_arg(cmd, void *));
	va_end(ap);
	pthread_testcancel();
	return (rv);
}
#endif

int
fsync(int fd)
{
	int rv;

	pthread_testcancel();
	rv = _thread_sys_fsync(fd);
	pthread_testcancel();
	return (rv);
}

int
msync(void *addr, size_t len, int flags)
{
	int rv;

	pthread_testcancel();
	rv =  _thread_sys_msync(addr, len, flags);
	pthread_testcancel();
	return (rv);
}

int
nanosleep(const struct timespec *rqtp, struct timespec *rmtp)
{
	int rv;

	pthread_testcancel();
	rv = _thread_sys_nanosleep(rqtp, rmtp);
	pthread_testcancel();
	return (rv);
}

#if 0
int
open(const char *path, int flags, ...)
{

	va_list ap;
	int rv;

	pthread_testcancel();
	va_start(ap, cmd);
	rv = _thread_sys_open(fd, cmd, va_arg(cmd, mode_t));
	va_end(ap);
	pthread_testcancel();
	return (rv);
}
#endif

#if 0
int
pause(void)
{

}
#endif

ssize_t
read(int fd, void *buf, size_t nbytes)
{
	ssize_t rv;

	pthread_testcancel();
	rv = read(fd, buf, nbytes);
	pthread_testcancel();
	return (rv);
}

#if 0
int
sigwaitinfo()
{

}
#endif

int
sigsuspend(const sigset_t *sigmask)
{
	int rv;

	pthread_testcancel();
	rv = sigsuspend(sigmask);
	pthread_testcancel();
	return (rv);
}

#if 0
/* libc sleep(3) calls nanosleep(2), so we'll catch it there */
unsigned int
sleep(unsigned int seconds)
{

}
#endif

#if 0
int system(const char *string)
{

}
#endif

#if 0
int
tcdrain(int fd)
{

}
#endif

#if 0
/* wait and waitpid will be handled by libc calling wait4 */
pid_t
wait(int *status)
{

}

pid_t
waitpid(pid_t wpid, int *status, int options)
{

}
#endif

pid_t
wait4(pid_t wpid, int *status, int options, struct rusage *rusage)
{
	pid_t rv;

	pthread_testcancel();
	rv = _thread_sys_wait4(wpid, status, options, rusage);
	pthread_testcancel();
	return (rv);
}

ssize_t
write(int fd, const void *buf, size_t nbytes)
{
	ssize_t rv;

	pthread_testcancel();
	rv = _thread_sys_write(fd, buf, nbytes);
	pthread_testcancel();
	return (rv);
}
#endif

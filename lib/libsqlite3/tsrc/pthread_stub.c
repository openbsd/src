/* stubs for pthreads function, quick and dirty */
#if SQLITE_THREADSAFE && !defined(SQLITE_TEST)

#include <pthread.h>

#define WEAKALIAS(f,g ) extern f __attribute__((__weak__, __alias__(#g)))

static pthread_t _sqlite_self_stub()
{
	return 0;
}

static int _sqlite_zero_stub()
{
	return 0;
}

WEAKALIAS(pthread_t pthread_self(void), _sqlite_self_stub);
WEAKALIAS(int pthread_mutex_init(pthread_mutex_t *a, const pthread_mutexattr_t *b), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutex_destroy(pthread_mutex_t *a), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutex_lock(pthread_mutex_t *a), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutex_trylock(pthread_mutex_t *a), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutex_unlock(pthread_mutex_t *a), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutexattr_init(pthread_mutexattr_t *a), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutexattr_settype(pthread_mutexattr_t *a, int b), _sqlite_zero_stub);
WEAKALIAS(int pthread_mutexattr_destroy(pthread_mutexattr_t *a), _sqlite_zero_stub);

#endif

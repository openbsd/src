/* ==== test_pthread_cond.c =========================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_cond(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include "test.h"

int contention_variable;

void * 
thread_contention(arg)
	void *arg;
{
	pthread_mutex_t *mutex = arg;

	CHECKr(pthread_mutex_lock(mutex));
	ASSERT(contention_variable == 1);
	contention_variable = 2;
	CHECKr(pthread_mutex_unlock(mutex));
	pthread_exit(NULL);
}

void
test_contention_lock(mutex)
	pthread_mutex_t *mutex;
{
	pthread_t thread;

	printf("  test_contention_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	contention_variable = 0;
	CHECKr(pthread_create(&thread, NULL, thread_contention, mutex));
	pthread_set_name_np(thread, "cntntn");
	pthread_yield();
	contention_variable = 1;
	CHECKr(pthread_mutex_unlock(mutex));
	CHECKr(pthread_mutex_lock(mutex));
	ASSERT(contention_variable == 2);
	CHECKr(pthread_mutex_unlock(mutex));
}

void
test_nocontention_lock(mutex)
	pthread_mutex_t *mutex;
{
	printf("  test_nocontention_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
}

void
test_debug_double_lock(mutex)
	pthread_mutex_t *mutex;
{
	printf("  test_debug_double_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	ASSERTe(pthread_mutex_lock(mutex), == EDEADLK);
	CHECKr(pthread_mutex_unlock(mutex));
}

void
test_debug_double_unlock(mutex)
	pthread_mutex_t *mutex;
{
	printf("  test_debug_double_unlock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
	ASSERTe(pthread_mutex_unlock(mutex), == EPERM);
}

void
test_nocontention_trylock(mutex)
	pthread_mutex_t *mutex;
{
	printf("  test_nocontention_trylock()\n");
	CHECKr(pthread_mutex_trylock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
}

void
test_mutex_static()
{
	pthread_mutex_t mutex_static = PTHREAD_MUTEX_INITIALIZER;

	printf("test_mutex_static()\n");
	test_nocontention_lock(&mutex_static);
	test_contention_lock(&mutex_static);
}

void
test_mutex_fast(void)
{
	pthread_mutex_t mutex_fast; 

	printf("test_mutex_fast()\n");
	CHECKr(pthread_mutex_init(&mutex_fast, NULL));
	test_nocontention_lock(&mutex_fast);
	test_contention_lock(&mutex_fast);
	CHECKr(pthread_mutex_destroy(&mutex_fast));
}

void
test_mutex_debug()
{
	pthread_mutexattr_t mutex_debug_attr; 
	pthread_mutex_t mutex_debug; 

	printf("test_mutex_debug()\n");
	CHECKr(pthread_mutexattr_init(&mutex_debug_attr));
	CHECKr(pthread_mutexattr_settype(&mutex_debug_attr, 
	    PTHREAD_MUTEX_ERRORCHECK));
	CHECKr(pthread_mutex_init(&mutex_debug, &mutex_debug_attr));
	test_nocontention_lock(&mutex_debug);
	test_contention_lock(&mutex_debug);
	test_debug_double_lock(&mutex_debug);
	test_debug_double_unlock(&mutex_debug);
	CHECKr(pthread_mutex_destroy(&mutex_debug));
}

int
main()
{
	test_mutex_static();
	test_mutex_fast();
	test_mutex_debug();
	SUCCEED;
}

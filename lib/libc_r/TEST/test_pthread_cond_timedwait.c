/* ==== test_pthread_cond.c =========================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_cond(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include "test.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void* thread_1(void * new_buf)
{
	CHECKr(pthread_mutex_lock(&mutex));
	CHECKr(pthread_cond_signal(&cond));
	CHECKr(pthread_mutex_unlock(&mutex));
	pthread_exit(NULL);
}

void* thread_2(void * new_buf)
{
	sleep(1);
	CHECKr(pthread_mutex_lock(&mutex));
	CHECKr(pthread_cond_signal(&cond));
	CHECKr(pthread_mutex_unlock(&mutex));
	pthread_exit(NULL);
}

int
main()
{
	struct timespec abstime = { 0, 0 };
	struct timeval curtime;
	pthread_t thread;
	int ret;

	printf("pthread_cond_timedwait START\n");

	CHECKr(pthread_mutex_lock(&mutex));
	CHECKe(gettimeofday(&curtime, NULL));
	abstime.tv_sec = curtime.tv_sec + 5; 

	/* Test a condition timeout */
	switch((ret = pthread_cond_timedwait(&cond, &mutex, &abstime))) {
	case 0:
		PANIC("pthread_cond_timedwait #0 failed to timeout");
		/* NOTREACHED */
	case ETIMEDOUT:
		/* expected behaviour */
		printf("Got first timeout ok\n");	/* Added by monty */
		break;
	default:
		DIE(ret, "pthread_cond_timedwait");
		/* NOTREACHED */
	}

	/* Test a normal condition signal */
	CHECKr(pthread_create(&thread, NULL, thread_1, NULL));

	abstime.tv_sec = curtime.tv_sec + 10; 
	CHECKr(pthread_cond_timedwait(&cond, &mutex, &abstime));

	/* Test a normal condition signal after a sleep */
	CHECKr(pthread_create(&thread, NULL, thread_2, NULL));

	pthread_yield();

	abstime.tv_sec = curtime.tv_sec + 10; 
	CHECKr(pthread_cond_timedwait(&cond, &mutex, &abstime));

	SUCCEED;
}

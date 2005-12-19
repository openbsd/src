/*	$OpenBSD: pthread_mutex.c,v 1.6 2005/12/19 05:22:57 tedu Exp $	*/
/*
 * Copyright (c) 1993, 1994, 1995, 1996 by Chris Provenzano and contributors, 
 * proven@mit.edu All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Chris Provenzano,
 *	the University of California, Berkeley, and contributors.
 * 4. Neither the name of Chris Provenzano, the University, nor the names of
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO, THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */ 

/* ==== test_pthread_cond.c =========================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_mutex(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include "test.h"

int contention_variable;

static void * 
thread_contention(void *arg)
{
	pthread_mutex_t *mutex = arg;

	SET_NAME("cntntn");

	CHECKr(pthread_mutex_lock(mutex));
	ASSERT(contention_variable == 1);
	contention_variable = 2;
	CHECKr(pthread_mutex_unlock(mutex));
	pthread_exit(NULL);
}

static void
test_contention_lock(pthread_mutex_t *mutex)
{
	pthread_t thread;

	printf("  test_contention_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	contention_variable = 0;
	CHECKr(pthread_create(&thread, NULL, thread_contention, mutex));
	pthread_yield();
	contention_variable = 1;
	CHECKr(pthread_mutex_unlock(mutex));
	pthread_yield();
	CHECKr(pthread_mutex_lock(mutex));
	ASSERT(contention_variable == 2);
	CHECKr(pthread_mutex_unlock(mutex));
}

static void
test_nocontention_lock(pthread_mutex_t *mutex)
{
	printf("  test_nocontention_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
}

static void
test_debug_double_lock(pthread_mutex_t *mutex)
{
	printf("  test_debug_double_lock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	ASSERTe(pthread_mutex_lock(mutex), == EDEADLK);
	CHECKr(pthread_mutex_unlock(mutex));
}

static void
test_debug_double_unlock(pthread_mutex_t *mutex)
{
	printf("  test_debug_double_unlock()\n");
	CHECKr(pthread_mutex_lock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
	/* Posix D10 says undefined behaviour? */
	ASSERTe(pthread_mutex_unlock(mutex), != 0);
}

static void
test_nocontention_trylock(pthread_mutex_t *mutex)
{
	printf("  test_nocontention_trylock()\n");
	CHECKr(pthread_mutex_trylock(mutex));
	CHECKr(pthread_mutex_unlock(mutex));
}

static void
test_mutex_static(void)
{
	pthread_mutex_t mutex_static = PTHREAD_MUTEX_INITIALIZER;

	printf("test_mutex_static()\n");
	test_nocontention_lock(&mutex_static);
	test_nocontention_trylock(&mutex_static);
	test_contention_lock(&mutex_static);
}

static void
test_mutex_fast(void)
{
	pthread_mutex_t mutex_fast; 

	printf("test_mutex_fast()\n");
	CHECKr(pthread_mutex_init(&mutex_fast, NULL));
	test_nocontention_lock(&mutex_fast);
	test_nocontention_trylock(&mutex_fast);
	test_contention_lock(&mutex_fast);
	CHECKr(pthread_mutex_destroy(&mutex_fast));
}

static void
test_mutex_debug(void)
{
	pthread_mutexattr_t mutex_debug_attr; 
	pthread_mutex_t mutex_debug; 

	printf("test_mutex_debug()\n");
	CHECKr(pthread_mutexattr_init(&mutex_debug_attr));
	CHECKr(pthread_mutexattr_settype(&mutex_debug_attr, 
	    PTHREAD_MUTEX_ERRORCHECK));
	CHECKr(pthread_mutex_init(&mutex_debug, &mutex_debug_attr));
	test_nocontention_lock(&mutex_debug);
	test_nocontention_trylock(&mutex_debug);
	test_contention_lock(&mutex_debug);
	test_debug_double_lock(&mutex_debug);
	test_debug_double_unlock(&mutex_debug);
	CHECKr(pthread_mutex_destroy(&mutex_debug));
}

static void
test_mutex_recursive(void)
{
	pthread_mutexattr_t mutex_recursive_attr; 
	pthread_mutex_t mutex_recursive; 
	int i;
	int j = 9;

	printf("test_mutex_recursive()\n");
	CHECKr(pthread_mutexattr_init(&mutex_recursive_attr));
	CHECKr(pthread_mutexattr_settype(&mutex_recursive_attr, 
	    PTHREAD_MUTEX_RECURSIVE));
	CHECKr(pthread_mutex_init(&mutex_recursive, &mutex_recursive_attr));

	CHECKr(pthread_mutex_lock(&mutex_recursive));
	for (i = 0; i < j; i++)
		CHECKr(pthread_mutex_lock(&mutex_recursive));
	for (i = 0; i < j; i++)
		CHECKr(pthread_mutex_unlock(&mutex_recursive));
	CHECKr(pthread_mutex_unlock(&mutex_recursive));
	/* Posix D10 says undefined behaviour? */
	ASSERTe(pthread_mutex_unlock(&mutex_recursive), != 0);
	CHECKr(pthread_mutex_destroy(&mutex_recursive));
}

int
main(int argc, char *argv[])
{
	test_mutex_static();
	test_mutex_fast();
	test_mutex_debug();
	test_mutex_recursive();
	SUCCEED;
}

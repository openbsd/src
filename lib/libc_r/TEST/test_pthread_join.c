/* ==== test_pthread_join.c =================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_join(). Run this after test_create()
 *
 *  1.23 94/05/04 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "test.h"

/* This thread yields so the creator has a live thread to wait on */
void* new_thread_1(void * new_buf)
{
	int i;

	sprintf((char *)new_buf, "New thread %%d stack at %p\n", &i);
	pthread_yield();	/* (ensure parent can wait on live thread) */
	sleep(1);
	return(new_buf);
	PANIC("return");
}

/* This thread doesn't yield so the creator has a dead thread to wait on */
void* new_thread_2(void * new_buf)
{
	int i;

	sprintf((char *)new_buf, "New thread %%d stack at %p\n", &i);
	return(new_buf);
	PANIC("return");
}

int
main()
{
	char buf[256], *status;
	pthread_t thread;
	int debug = 1;
	int i = 0;

	if (debug)
		printf("Original thread stack at %p\n", &i);

	CHECKr(pthread_create(&thread, NULL, new_thread_1, (void *)buf));
	CHECKr(pthread_join(thread, (void **)(&status)));
	if (debug) 
		printf(status, ++i);

	/* Now have the created thread finishing before the join. */
	CHECKr(pthread_create(&thread, NULL, new_thread_2, (void *)buf));
	pthread_yield();
	sleep(1); /* (ensure thread is dead) */
	CHECKr(pthread_join(thread, (void **)(&status)));

	if (debug)
		printf(status, ++i);

	SUCCEED;
}


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
#include "test.h"

void* new_thread(void * new_buf)
{
	int i;

	printf("new_thread\n");
	for (i = 0; i < 10; i++) {
		printf("yielding ");
		fflush(stdout);
		pthread_yield();
	}
	printf("yielded 10 times ok\n");
	exit(0);
}

int
main()
{
	pthread_t thread;
	int ret;

	printf("test_preemption START\n");

	if ((ret = pthread_create(&thread, NULL, new_thread, NULL)))
		DIE(ret, "pthread_create");

	while(1);
	/* NOTREACHED */

	return (1);
}

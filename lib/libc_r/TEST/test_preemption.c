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

	printf("yielding:");
	for (i = 0; i < 10; i++) {
		printf(" %d", i);
		fflush(stdout);
		pthread_yield();
	}
	printf("\n");
	SUCCEED;
}

int
main()
{
	pthread_t thread;

	CHECKr(pthread_create(&thread, NULL, new_thread, NULL));

	while(1)
		;
	PANIC("while");
}

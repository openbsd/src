/* ==== test_create.c ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Description : Test pthread_create() and pthread_exit() calls.
 *
 *  1.00 93/08/03 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>
#include "test.h"

void* new_thread(void* arg)
{
	int i;

	printf("New thread was passed arg address %p\n", arg);
	printf("New thread stack at %p\n", &i);
	return(NULL);
	PANIC("return");
}

int
main()
{
	pthread_t thread;
	int i;

	printf("Original thread stack at %p\n", &i);
	CHECKr(pthread_create(&thread, NULL, new_thread, 
	    (void *)0xdeadbeef));
	CHECKr(pthread_join(thread, NULL));
	SUCCEED;
}

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
	PANIC();
}

int
main()
{
	pthread_t thread;
	int i;
	int ret;

	printf("Original thread stack at %p\n", &i);
	if ((ret = pthread_create(&thread, NULL, new_thread, 
	    (void *)0xdeadbeef)))
		DIE(ret, "pthread_create");
	if ((ret = pthread_join(thread, NULL)))
		DIE(ret, "pthread_join");
	return(0);
}

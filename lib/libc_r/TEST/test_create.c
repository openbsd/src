/*	$OpenBSD: test_create.c,v 1.4 2000/01/06 06:52:45 d Exp $	*/
/*
 * Copyright (c) 1993 by Chris Provenzano, proven@athena.mit.edu
 *
 * Test pthread_create() and pthread_exit() calls.
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

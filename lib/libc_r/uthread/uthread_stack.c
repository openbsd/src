/*	$OpenBSD: uthread_stack.c,v 1.6 2000/02/26 13:34:51 d Exp $	*/
/*
 * Copyright 1999, David Leonard. All rights reserved.
 * <insert BSD-style license&disclaimer>
 */

/*
 * Thread stack allocation.
 *
 * Stack pointers are assumed to work their way down (backwards) to the
 * beginning of the stack storage. The first page of this storage is 
 * protected using mprotect() so as to generate a SIGSEGV if a thread
 * overflows its stack.
 */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <pthread.h>
#include <pthread_np.h>
#include "pthread_private.h"

struct stack *
_thread_stack_alloc(base, size)
	void *base;
	size_t size;
{
	struct stack *stack;
	int nbpg = getpagesize();

	/* Maintain a stack of default-sized stacks that we can re-use. */
	if (base == NULL && size == PTHREAD_STACK_DEFAULT) {
		if (pthread_mutex_lock(&_gc_mutex) != 0)
			PANIC("Cannot lock gc mutex");

		if ((stack = SLIST_FIRST(&_stackq)) != NULL) {
			SLIST_REMOVE_HEAD(&_stackq, qe);
			if (pthread_mutex_unlock(&_gc_mutex) != 0)
				PANIC("Cannot unlock gc mutex");
			return stack;
		}
		if (pthread_mutex_unlock(&_gc_mutex) != 0)
			PANIC("Cannot unlock gc mutex");
	}

	/* Allocate some storage to hold information about the stack: */
	stack = (struct stack *)malloc(sizeof (struct stack));
	if (stack == NULL) 
		return NULL;

	if (base != NULL) {
		/* Use the user's storage */
		stack->base = base;
		stack->size = size;
		stack->redzone = NULL;
		stack->storage = NULL;
		return stack;
	}

	/* Allocate some storage for the stack, with some overhead: */
	stack->storage = malloc(size + nbpg * 2);
	if (stack->storage == NULL) {
		free(stack);
		return NULL;
	}

	/*
	 * The red zone is the first physical page of the storage.
	 * I'm using _BSD_PTRDIFF_T_ to convert the storage base pointer
	 * into an integer so that I can do page alignment on it.
	 */
	stack->redzone = (void*)(((_BSD_PTRDIFF_T_)stack->storage + nbpg - 1) & 
	    ~(nbpg - 1));
	if (mprotect(stack->redzone, nbpg, 0) == -1)
		PANIC("Cannot protect stack red zone");

	/* Find the useful range of the stack. */
	stack->base = (caddr_t)stack->redzone + nbpg;
	stack->size = size;

	return stack;
}

void
_thread_stack_free(stack)
	struct stack *stack;
{
	int nbpg = getpagesize();

	/* Cache allocated stacks of default size. */
	if (stack->storage != NULL && stack->size == PTHREAD_STACK_DEFAULT)
		SLIST_INSERT_HEAD(&_stackq, stack, qe);
	else {
		/* Restore storage protection to what malloc expects: */
		if (stack->redzone)
			mprotect(stack->redzone, nbpg,
			    PROT_READ|PROT_WRITE);

		/* Free storage */
		if (stack->storage)
			free(stack->storage);

		/* Free stack information storage. */
		free(stack);
	}
}

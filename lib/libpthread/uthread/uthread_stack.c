/*	$OpenBSD: uthread_stack.c,v 1.10 2007/05/18 19:28:50 kurt Exp $	*/
/*
 * Copyright 1999, David Leonard. All rights reserved.
 * <insert BSD-style license&disclaimer>
 */

/*
 * Thread stack allocation.
 *
 * If stack pointers grow down, towards the beginning of stack storage,
 * the first page of the storage is protected using mprotect() so as
 * to generate a SIGSEGV if a thread overflows its stack. Similarly,
 * for stacks that grow up, the last page of the storage is protected.
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
	size_t nbpg = (size_t)getpagesize();

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

	/* Round size up to closest page boundry */
	size = ((size + (nbpg - 1)) / nbpg) * nbpg;

	/* mmap storage for the stack, with one extra page for redzone */
	stack->storage = mmap(NULL, size + nbpg, PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0);
	if (stack->storage == MAP_FAILED) {
		free(stack);
		return NULL;
	}

	/*
	 * Compute the location of the red zone.
	 */
#if defined(MACHINE_STACK_GROWS_UP)
	/* Red zone is the last page of the storage: */
	stack->redzone = (void *)((caddr_t)stack->storage + (ptrdiff_t)size);
	stack->base = stack->storage;
	stack->size = size;
#else
	/* Red zone is the first page of the storage: */
	stack->redzone = stack->storage; 
	stack->base = (caddr_t)stack->redzone + (ptrdiff_t)nbpg;
	stack->size = size;
#endif
	if (mprotect(stack->redzone, nbpg, PROT_NONE) == -1)
		PANIC("Cannot protect stack red zone");

	return stack;
}

void
_thread_stack_free(stack)
	struct stack *stack;
{
	size_t nbpg = (size_t)getpagesize();

	/* Cache allocated stacks of default size: */
	if (stack->storage != NULL && stack->size == PTHREAD_STACK_DEFAULT)
		SLIST_INSERT_HEAD(&_stackq, stack, qe);
	else {
		/* unmap storage: */
		if (stack->storage)
			munmap(stack->storage, stack->size + nbpg);

		/* Free stack information storage: */
		free(stack);
	}
}

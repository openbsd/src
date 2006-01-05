/* $OpenBSD: rthread_stack.c,v 1.2 2006/01/05 08:15:16 otto Exp $ */
/* $snafu: rthread_stack.c,v 1.12 2005/01/11 02:45:28 marc Exp $ */

/* PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */

#include <sys/types.h>
#include <sys/mman.h>

#include <machine/param.h>
#include <machine/spinlock.h>

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "rthread.h"

struct stack *
_rthread_alloc_stack(pthread_t thread)
{
	struct stack *stack;
	caddr_t base;
	caddr_t guard;
	caddr_t start = NULL;
	size_t pgsz;
	size_t size;

	/* guard pages are forced to a multiple of the page size */
	pgsz = sysconf(_SC_PAGESIZE);
	if (pgsz == (size_t)-1)
		return NULL;

	/* figure out the actual requested size, including guard size */
	size = thread->attr.stack_size + thread->attr.guard_size;
	size += pgsz - 1;
	size &= ~(pgsz - 1);

	/*
	 * Allocate some stack space unless and address was provided.
	 * A provided address is ASSUMED to be correct with respect to
	 * alignment constraints.
	 */
	if (size > thread->attr.guard_size) {
		if (thread->attr.stack_addr)
			base = thread->attr.stack_addr;
		else {
			base = mmap(NULL, size, PROT_READ | PROT_WRITE,
				    MAP_ANON, -1, 0);
			if (base == MAP_FAILED)
				return (NULL);
		}
		/* memory protect the guard region */

#ifdef MACHINE_STACK_GROWS_UP
		guard = base + size - thread->attr.guard_size;
		start = base;
#else
		guard = base;
		start = base + size;
#endif
		if (mprotect(guard, thread->attr.guard_size, PROT_NONE) == -1) {
			munmap(base, size);
			return (NULL);
		}

		/* wrap up the info in a struct stack and return it */
		stack = malloc(sizeof(*stack));
		if (!stack) {
			munmap(base, size);
			return (NULL);
		}
		stack->sp = start;
		stack->base = base;
		stack->guard = guard;
		stack->guardsize = thread->attr.guard_size;
		stack->len = size;
		return (stack);
	}
	return (NULL);
}

void
_rthread_free_stack(struct stack *stack)
{
	munmap(stack->base, stack->len);
	free(stack);
}


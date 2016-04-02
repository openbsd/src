/*	$OpenBSD: rthread_attr.c,v 1.21 2016/04/02 19:56:53 guenther Exp $ */
/*
 * Copyright (c) 2004,2005 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * generic attribute support
 */

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <pthread.h>
#include <pthread_np.h>

#include "rthread.h"

/*
 * Note: stack_size + guard_size == total stack used
 *
 * pthread_attr_init MUST be called before any other attribute function
 * for proper operation.
 *
 * Every call to pthread_attr_init MUST be matched with a call to
 * pthread_attr_destroy to avoid leaking memory.   This is an implementation
 * requirement, not a POSIX requirement.
 */

int
pthread_attr_init(pthread_attr_t *attrp)
{
	pthread_attr_t attr;
	int error;

	/* make sure _rthread_attr_default has been initialized */
	if (!_threads_ready)
		if ((error = _rthread_init()))
			return (error);

	attr = calloc(1, sizeof(*attr));
	if (!attr)
		return (errno);
	*attr = _rthread_attr_default;
	*attrp = attr;

	return (0);
}

int
pthread_attr_destroy(pthread_attr_t *attrp)
{
	free(*attrp);
	*attrp = NULL;

	return (0);
}

int
pthread_attr_getguardsize(const pthread_attr_t *attrp, size_t *guardsize)
{
	*guardsize = (*attrp)->guard_size;

	return (0);
}

int
pthread_attr_setguardsize(pthread_attr_t *attrp, size_t guardsize)
{
	(*attrp)->guard_size = guardsize;

	return (0);
}

int
pthread_attr_getdetachstate(const pthread_attr_t *attrp, int *detachstate)
{
	*detachstate = (*attrp)->detach_state;

	return (0);
}

int
pthread_attr_setdetachstate(pthread_attr_t *attrp, int detachstate)
{
	int error;

	error = (detachstate == PTHREAD_CREATE_DETACHED ||
		  detachstate == PTHREAD_CREATE_JOINABLE) ? 0 : EINVAL;
	if (error == 0)
		(*attrp)->detach_state = detachstate;

	return (error);
}

int
pthread_attr_getstack(const pthread_attr_t *attrp, void **stackaddr,
    size_t *stacksize)
{
	*stackaddr = (*attrp)->stack_addr;
	*stacksize = (*attrp)->stack_size;

	return (0);
}

int
pthread_attr_setstack(pthread_attr_t *attrp, void *stackaddr, size_t stacksize)
{
	int error;

	/*
	 * XXX Add an alignment test, on stackaddr for stack-grows-up
	 * archs or on stackaddr+stacksize for stack-grows-down archs
	 */
	if (stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);
	if ((error = pthread_attr_setstackaddr(attrp, stackaddr)))
		return (error);
	(*attrp)->stack_size = stacksize;

	return (0);
}

int
pthread_attr_getstacksize(const pthread_attr_t *attrp, size_t *stacksize)
{
	*stacksize = (*attrp)->stack_size;
	
	return (0);
}

int
pthread_attr_setstacksize(pthread_attr_t *attrp, size_t stacksize)
{
	if (stacksize < PTHREAD_STACK_MIN ||
	    stacksize > ROUND_TO_PAGE(stacksize))
		return (EINVAL);
	(*attrp)->stack_size = stacksize;

	return (0);
}

int
pthread_attr_getstackaddr(const pthread_attr_t *attrp, void **stackaddr)
{
	*stackaddr = (*attrp)->stack_addr;

	return (0);
}

int
pthread_attr_setstackaddr(pthread_attr_t *attrp, void *stackaddr)
{
	if (stackaddr == NULL || (uintptr_t)stackaddr & (_thread_pagesize - 1))
		return (EINVAL);
	(*attrp)->stack_addr = stackaddr;

	return (0);
}
DEF_NONSTD(pthread_attr_setstackaddr);

int
pthread_attr_getscope(const pthread_attr_t *attrp, int *contentionscope)
{
	*contentionscope = (*attrp)->contention_scope;

	return (0);
}

int
pthread_attr_setscope(pthread_attr_t *attrp, int contentionscope)
{
	if (contentionscope != PTHREAD_SCOPE_SYSTEM &&
	    contentionscope != PTHREAD_SCOPE_PROCESS)
		return (EINVAL);
	(*attrp)->contention_scope = contentionscope;

	return (0);
}


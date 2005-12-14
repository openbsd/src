/*	$OpenBSD: rthread.c,v 1.5 2005/12/14 04:43:04 tedu Exp $ */
/*
 * Copyright (c) 2004 Ted Unangst <tedu@openbsd.org>
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
 * The heart of rthreads.  Basic functions like creating and joining
 * threads.
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/spinlock.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

static int threads_ready;
static pthread_t thread_list;
static _spinlock_lock_t thread_lock;

int getthrid();
void threxit(int);
int rfork_thread(int, void *, void (*)(void *), void *);

/*
 * internal support functions
 */
void
_spinlock(_spinlock_lock_t *lock)
{

	while (_atomic_lock(lock))
		pthread_yield();
}

void
_spinunlock(_spinlock_lock_t *lock)
{

	*lock = _SPINLOCK_UNLOCKED;
}

static pthread_t
thread_findself(void)
{
	pthread_t me;
	pid_t tid = getthrid();

	for (me = thread_list; me; me = me->next)
		if (me->tid == tid)
			break;

	return (me);
}


static void
thread_start(void *v)
{
	pthread_t thread = v;
	void *retval;

	/* ensure parent returns from rfork, sets up tid */
	_spinlock(&thread_lock);
	_spinunlock(&thread_lock);
	retval = thread->fn(thread->arg);
	pthread_exit(retval);
}

static void
thread_init(void)
{
	pthread_t thread;
	extern int __isthreaded;

	printf("rthread init\n");

	__isthreaded = 1;

	thread = malloc(sizeof(*thread));
	memset(thread, 0, sizeof(*thread));
	thread->tid = getthrid();
	thread->flags |= THREAD_CANCEL_ENABLE|THREAD_CANCEL_DEFERRED;
	snprintf(thread->name, sizeof(thread->name), "Main process");
	thread_list = thread;
	threads_ready = 1;
}

static struct stack *
alloc_stack(size_t len)
{
	struct stack *stack;

	stack = malloc(sizeof(*stack));
	stack->base = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
	stack->sp = (void *)(((size_t)stack->base + len - 16) & ~15);
	return (stack);
}

/*
 * real pthread functions
 */
pthread_t
pthread_self(void)
{
	pthread_t thread;

	if (!threads_ready)
		thread_init();

	_spinlock(&thread_lock);
	thread = thread_findself();
	_spinunlock(&thread_lock);

	return (thread);
}

void
pthread_exit(void *retval)
{
	pthread_t thread = pthread_self();

	thread->retval = retval;
	thread->flags |= THREAD_DONE;
	
	_sem_post(&thread->donesem);
	rthread_tls_destructors(thread);
#if 0
	if (thread->flags & THREAD_DETACHED)
		free(thread);
#endif
	threxit(0);
	for(;;);
}

int
pthread_join(pthread_t thread, void **retval)
{

	_sem_wait(&thread->donesem, 0, 0);
	if (retval)
		*retval = thread->retval;

	return (0);
}

int
pthread_detach(pthread_t thread)
{
	_spinlock(&thread_lock);
#if 0
	if (thread->flags & THREAD_DONE)
		free(thread);
	else
#endif
		thread->flags |= THREAD_DETACHED;
	_spinunlock(&thread_lock);
	return (0);
}

int
pthread_create(pthread_t *threadp, const pthread_attr_t *attr,
    void *(*start_routine)(void *), void *arg)
{
	pthread_t thread;
	pid_t tid;

	if (!threads_ready)
		thread_init();

	thread = malloc(sizeof(*thread));
	memset(thread, 0, sizeof(*thread));
	thread->fn = start_routine;
	thread->arg = arg;

	_spinlock(&thread_lock);
	thread->next = thread_list;
	thread_list = thread;

	thread->stack = alloc_stack(64 * 1024);

	tid = rfork_thread(RFPROC | RFTHREAD | RFMEM | RFNOWAIT,
	    thread->stack->sp, thread_start, thread);
	/* new thread will appear thread_start */
	thread->tid = tid;
	thread->flags |= THREAD_CANCEL_ENABLE|THREAD_CANCEL_DEFERRED;
	*threadp = thread;
	_spinunlock(&thread_lock);

	return (0);
}

int
pthread_equal(pthread_t t1, pthread_t t2)
{
	return (t1 == t2);
}

int
pthread_cancel(pthread_t thread)
{

	thread->flags |= THREAD_CANCELLED;
	return (0);
}

void
pthread_testcancel(void)
{
	if ((pthread_self()->flags & (THREAD_CANCELLED|THREAD_CANCEL_ENABLE)) ==
	    (THREAD_CANCELLED|THREAD_CANCEL_ENABLE))
		pthread_exit(PTHREAD_CANCELED);

}

int
pthread_setcancelstate(int state, int *oldstatep)
{
	pthread_t self = pthread_self();
	int oldstate;

	oldstate = self->flags & THREAD_CANCEL_ENABLE ?
	    PTHREAD_CANCEL_ENABLE : PTHREAD_CANCEL_DISABLE;
	if (state == PTHREAD_CANCEL_ENABLE) {
		self->flags |= THREAD_CANCEL_ENABLE;
		pthread_testcancel();
	} else if (state == PTHREAD_CANCEL_DISABLE) {
		self->flags &= ~THREAD_CANCEL_ENABLE;
	} else {
		return (EINVAL);
	}
	if (oldstatep)
		*oldstatep = oldstate;

	return (0);
}

int
pthread_setcanceltype(int type, int *oldtypep)
{
	pthread_t self = pthread_self();
	int oldtype;

	oldtype = self->flags & THREAD_CANCEL_DEFERRED ?
	    PTHREAD_CANCEL_DEFERRED : PTHREAD_CANCEL_ASYNCHRONOUS;
	if (type == PTHREAD_CANCEL_DEFERRED) {
		self->flags |= THREAD_CANCEL_DEFERRED;
		pthread_testcancel();
	} else if (type == PTHREAD_CANCEL_ASYNCHRONOUS) {
		self->flags &= ~THREAD_CANCEL_DEFERRED;
	} else {
		return (EINVAL);
	}
	if (oldtypep)
		*oldtypep = oldtype;

	return (0);
}

/*
 * _np functions
 */
void
pthread_set_name_np(pthread_t thread, char *name)
{
	strlcpy(thread->name, name, sizeof(thread->name));
}

/*
 * compat debug stuff
 */
void
_thread_dump_info(void)
{
	pthread_t thread;

	_spinlock(&thread_lock);
	for (thread = thread_list; thread; thread = thread->next)
		printf("thread %d flags %d name %s\n",
		    thread->tid, thread->flags, thread->name);
	_spinunlock(&thread_lock);
}


/*
 * the malloc lock
 */
static _spinlock_lock_t malloc_lock;

void
_thread_malloc_lock()
{
	_spinlock(&malloc_lock);
}

void
_thread_malloc_unlock()
{
	_spinunlock(&malloc_lock);
}

void
_thread_malloc_init()
{
}

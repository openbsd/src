/*	$OpenBSD: rthread_fork.c,v 1.3 2009/11/27 19:42:24 guenther Exp $ */

/*
 * Copyright (c) 2008 Kurt Miller <kurt@openbsd.org>
 * Copyright (c) 2008 Philip Guenther <guenther@gmail.com>
 * Copyright (c) 2003 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/lib/libc_r/uthread/uthread_atfork.c,v 1.1 2004/12/10 03:36:45 grog Exp $
 */

#include <sys/param.h>

#include <machine/spinlock.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "thread_private.h"	/* in libc/include */

#include "rthread.h"

struct rthread_atfork {
	TAILQ_ENTRY(rthread_atfork) next;
	void (*prepare)(void);
	void (*parent)(void);
	void (*child)(void);
};

static TAILQ_HEAD(atfork_listhead, rthread_atfork) _atfork_list =
    TAILQ_HEAD_INITIALIZER(_atfork_list);

static _spinlock_lock_t _atfork_lock = _SPINLOCK_UNLOCKED;

pid_t   _thread_sys_fork(void);
pid_t   _thread_sys_vfork(void);
pid_t	_dofork(int);

pid_t
_dofork(int is_vfork)
{
	pthread_t me;
	pid_t (*sys_fork)(void);
	pid_t newid;

	sys_fork = is_vfork ? &_thread_sys_vfork : &_thread_sys_fork;

	if (!_threads_ready)
		return sys_fork();

	me = pthread_self();

	/*
	 * Protect important libc/ld.so critical areas across the fork call.
	 * dlclose() will grab the atexit lock via __cxa_finalize() so lock
	 * the dl_lock first. malloc()/free() can grab the arc4 lock so lock
	 * malloc_lock first. Finally lock the bind_lock last so that any lazy
	 * binding in the other locking functions can succeed.
	 */

#if defined(__ELF__) && defined(PIC)
	_rthread_dl_lock(0);
#endif

	_thread_atexit_lock();
	_thread_malloc_lock();
	_thread_arc4_lock();

#if defined(__ELF__) && defined(PIC)
	_rthread_bind_lock(0);
#endif

	newid = sys_fork();

#if defined(__ELF__) && defined(PIC)
	_rthread_bind_lock(1);
#endif

	_thread_arc4_unlock();
	_thread_malloc_unlock();
	_thread_atexit_unlock();

#if defined(__ELF__) && defined(PIC)
	_rthread_dl_lock(1);
#endif

	if (newid == 0) {
		/* update this thread's structure */
		me->tid = getthrid();
		me->donesem.lock = _SPINLOCK_UNLOCKED;
		me->flags &= ~THREAD_DETACHED;
		me->flags_lock = _SPINLOCK_UNLOCKED;

		/* this thread is the initial thread for the new process */
		_initial_thread = *me;

		/* reinit the thread list */
		LIST_INIT(&_thread_list);
		LIST_INSERT_HEAD(&_thread_list, &_initial_thread, threads);
		_thread_lock = _SPINLOCK_UNLOCKED;

		/* single threaded now */
		__isthreaded = 0;
	}
	return newid;
}

pid_t
fork(void)
{
	struct rthread_atfork *p;
	pid_t newid;

	_spinlock(&_atfork_lock);
	TAILQ_FOREACH_REVERSE(p, &_atfork_list, atfork_listhead, next)
		if (p->prepare)
			p->prepare();
	newid = _dofork(0);
	if (newid == 0) {
		TAILQ_FOREACH(p, &_atfork_list, next)
			if (p->child)
				p->child();
	} else {
		TAILQ_FOREACH(p, &_atfork_list, next)
			if (p->parent)
				p->parent();
	}
	_spinunlock(&_atfork_lock);
	return newid;
}

pid_t
vfork(void)
{
	return _dofork(1);
}

int
pthread_atfork(void (*prepare)(void), void (*parent)(void),
    void (*child)(void))
{
	struct rthread_atfork *af;

	if ((af = malloc(sizeof *af)) == NULL)
		return (ENOMEM);

	af->prepare = prepare;
	af->parent = parent;
	af->child = child;
	_spinlock(&_atfork_lock);
	TAILQ_INSERT_TAIL(&_atfork_list, af, next);
	_spinunlock(&_atfork_lock);
	return (0);
}

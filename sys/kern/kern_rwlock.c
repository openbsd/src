/*	$OpenBSD: kern_rwlock.c,v 1.2 2003/11/18 18:12:14 tedu Exp $	*/
/*
 * Copyright (c) 2002, 2003 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/rwlock.h>

/* XXX - temporary measure until proc0 is properly aligned */
#define RW_PROC(p) (((unsigned long)p) & ~RWLOCK_MASK)

#ifndef __HAVE_MD_RWLOCK
/*
 * Simple cases that should be in MD code and atomic.
 */
void
rw_enter_read(struct rwlock *rwl)
{
	while (__predict_false(rwl->rwl_owner & RWLOCK_WRLOCK)) {
		/*
		 * Not the simple case, go to slow path.
		 */
		rw_enter_wait(rwl, curproc, RW_READ);
	}
	rwl->rwl_owner += RWLOCK_READ_INCR;
}

void
rw_enter_write(struct rwlock *rwl, struct proc *p)
{
	while (__predict_false(rwl->rwl_owner != 0)) {
		/*
		 * Not the simple case, go to slow path.
		 */
		rw_enter_wait(rwl, p, RW_WRITE);
	}
	rwl->rwl_owner = RW_PROC(p) | RWLOCK_WRLOCK;
}

void
rw_exit_read(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;
	unsigned long decr = (owner & (RWLOCK_WAIT|RWLOCK_WRWANT)) |
	    RWLOCK_READ_INCR;

	rwl->rwl_owner -= decr;
	/*
	 * Potential MP race here. If the owner had WRWANT set we cleared
	 * it and a reader can sneak in before a writer. Do we care?
	 */
	if (__predict_false(owner & RWLOCK_WAIT))
		rw_exit_waiters(rwl, owner);
}

void
rw_exit_write(struct rwlock *rwl)
{
	unsigned long owner = rwl->rwl_owner;

	rwl->rwl_owner = 0;
	/*
	 * Potential MP race here. If the owner had WRWANT set we cleared
	 * it and a reader can sneak in before a writer. Do we care?
	 */
	if (__predict_false(owner & RWLOCK_WAIT))
		rw_exit_waiters(rwl, owner);
}
#endif

void
rw_init(struct rwlock *rwl)
{
	rwl->rwl_owner = 0;
}

void
rw_enter_wait(struct rwlock *rwl, struct proc *p, int how)
{
	unsigned long need_wait, set_wait;
	int wait_prio;

#ifdef DIAGNOSTIC
	if (p == NULL)
		panic("rw_enter_wait: NULL proc");
#endif

	/*
	 * XXX - this function needs a lot of help to become MP safe.
	 */

	switch (how) {
	case RW_READ:
		/*
		 * Let writers through before obtaining read lock.
		 */
		need_wait = RWLOCK_WRLOCK | RWLOCK_WRWANT;
		set_wait = RWLOCK_WAIT;
		wait_prio = PLOCK;
		break;
	case RW_WRITE:
		need_wait = ~0UL;
		set_wait = RWLOCK_WAIT | RWLOCK_WRWANT;
		wait_prio = PLOCK - 4;
		if (RW_PROC(RWLOCK_OWNER(rwl)) == RW_PROC(p)) {
			panic("rw_enter: locking against myself");
		}
		break;
	}

	while (rwl->rwl_owner & need_wait) {
		rwl->rwl_owner |= set_wait;
		tsleep(rwl, wait_prio, "rwlock", 0);
	}
}

void
rw_exit_waiters(struct rwlock *rwl, unsigned long owner)
{
#ifdef DIAGNOSTIC
	if ((owner & RWLOCK_WAIT) == 0)
		panic("rw_exit_waiters: no waiter");
#endif
	/* We wake up all waiters because we can't know how many they are. */
	wakeup(rwl);	
}

#ifdef RWLOCK_TEST
#include <sys/kthread.h>

void rwlock_test(void);

void rwlock_testp1(void *);
void rwlock_testp2(void *);
void rwlock_testp3(void *);

struct rwlock rw_test = RWLOCK_INITIALIZER;

void
rwlock_test(void)
{
	kthread_create(rwlock_testp1, NULL, NULL, "rw1");
	kthread_create(rwlock_testp2, NULL, NULL, "rw2");
	kthread_create(rwlock_testp3, NULL, NULL, "rw3");
}

void
rwlock_testp1(void *a)
{
	int local;

	printf("rwlock test1 start\n");
	rw_enter_read(&rw_test);
	printf("rwlock test1 obtained\n");
	tsleep(&local, PWAIT, "rw1", 4);
	rw_exit_read(&rw_test);
	printf("rwlock test1 released\n");
	tsleep(&local, PWAIT, "rw1/2", 3);
	rw_enter_read(&rw_test);
	printf("rwlock test1 obtained\n");
	rw_exit_read(&rw_test);
	printf("rwlock test1 released\n");
	kthread_exit(0);
}

void
rwlock_testp2(void *a)
{
	int local;

	printf("rwlock test2 start\n");
	rw_enter_read(&rw_test);
	printf("rwlock test2 obtained\n");
	tsleep(&local, PWAIT, "rw2", 4);
	rw_exit_read(&rw_test);
	printf("rwlock test2 released\n");
	kthread_exit(0);
}

void
rwlock_testp3(void *a)
{
	int local;

	printf("rwlock test3 start\n");
	tsleep(&local, PWAIT, "rw3", 2);
	printf("rwlock test3 exited waiting\n");
	rw_enter_write(&rw_test, curproc);
	printf("rwlock test3 obtained\n");
	tsleep(&local, PWAIT, "rw3/2", 4);
	rw_exit_write(&rw_test);
	printf("rwlock test3 released\n");
	kthread_exit(0);
}
#endif

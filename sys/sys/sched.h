/*	$OpenBSD: sched.h,v 1.6 2004/06/13 21:49:28 niklas Exp $	*/
/* $NetBSD: sched.h,v 1.2 1999/02/28 18:14:58 ross Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_clock.c	8.5 (Berkeley) 1/21/94
 */

#ifndef	_SYS_SCHED_H_
#define	_SYS_SCHED_H_

/*
 * Posix defines a <sched.h> which may want to include <sys/sched.h>
 */

#ifdef	_KERNEL

#define	PPQ	(128 / NQS)		/* priorities per queue */
#define NICE_WEIGHT 2			/* priorities per nice level */
#define	ESTCPULIM(e) min((e), NICE_WEIGHT * PRIO_MAX - PPQ)

extern int schedhz;			/* ideally: 16 */
extern int rrticks_init;		/* ticks per roundrobin() */

#ifdef	_SYS_PROC_H_
void schedclock(struct proc *);
#ifdef __HAVE_CPUINFO
void roundrobin(struct cpu_info *);
#endif
static __inline void scheduler_fork_hook(
	struct proc *parent, struct proc *child);
static __inline void scheduler_wait_hook(
	struct proc *parent, struct proc *child);

/* Inherit the parent's scheduler history */

static __inline void
scheduler_fork_hook(parent, child)
	struct proc *parent, *child;
{
	child->p_estcpu = parent->p_estcpu;
}

/* Chargeback parents for the sins of their children.  */

static __inline void
scheduler_wait_hook(parent, child)
	struct proc *parent, *child;
{
	/* XXX just return if parent == init?? */

	parent->p_estcpu = ESTCPULIM(parent->p_estcpu + child->p_estcpu);
}
#endif	/* _SYS_PROC_H_ */

#ifndef splsched
#define splsched() splhigh()
#endif
#ifndef IPL_SCHED
#define IPL_SCHED IPL_HIGH
#endif

#if defined(MULTIPROCESSOR) || defined(LOCKDEBUG)
#include <sys/lock.h>

/*
 * XXX Instead of using struct lock for the kernel lock and thus requiring us
 * XXX to implement simplelocks, causing all sorts of fine-grained locks all
 * XXX over our tree getting activated consuming both time and potentially
 * XXX introducing locking protocol bugs.
 */
#ifdef notyet

extern struct simplelock sched_lock;

#define	SCHED_ASSERT_LOCKED()	LOCK_ASSERT(simple_lock_held(&sched_lock))
#define	SCHED_ASSERT_UNLOCKED()	LOCK_ASSERT(simple_lock_held(&sched_lock) == 0)

#define	SCHED_LOCK(s)							\
do {									\
	s = splsched();							\
	simple_lock(&sched_lock);					\
} while (/* CONSTCOND */ 0)

#define	SCHED_UNLOCK(s)							\
do {									\
	simple_unlock(&sched_lock);					\
	splx(s);							\
} while (/* CONSTCOND */ 0)

#else

extern struct __mp_lock sched_lock;

#define	SCHED_ASSERT_LOCKED()	LOCK_ASSERT(__mp_lock_held(&sched_lock))
#define	SCHED_ASSERT_UNLOCKED()	LOCK_ASSERT(__mp_lock_held(&sched_lock) == 0)

#define	SCHED_LOCK(s)							\
do {									\
	s = splsched();							\
	__mp_lock(&sched_lock);						\
} while (/* CONSTCOND */ 0)

#define	SCHED_UNLOCK(s)							\
do {									\
	__mp_unlock(&sched_lock);					\
	splx(s);							\
} while (/* CONSTCOND */ 0)

#endif

void	sched_lock_idle(void);
void	sched_unlock_idle(void);

#else /* ! MULTIPROCESSOR || LOCKDEBUG */

#define	SCHED_ASSERT_LOCKED()		splassert(IPL_SCHED);
#define	SCHED_ASSERT_UNLOCKED()		/* nothing */

#define	SCHED_LOCK(s)			s = splsched()
#define	SCHED_UNLOCK(s)			splx(s)

#endif /* MULTIPROCESSOR || LOCKDEBUG */

#endif	/* _KERNEL */
#endif	/* _SYS_SCHED_H_ */

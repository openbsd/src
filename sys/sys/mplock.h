/*
 * Copyright (c) 2004 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _MPLOCK_H_
#define _MPLOCK_H_

/*
 * Really simple spinlock implementation with recursive capabilities.
 * Correctness is paramount, no fancyness allowed.
 */

struct __mp_lock {
	__cpu_simple_lock_t mpl_lock;
	cpuid_t	mpl_cpu;
	int	mpl_count;
};

static __inline void __mp_lock_init(struct __mp_lock *);
static __inline void __mp_lock(struct __mp_lock *);
static __inline void __mp_unlock(struct __mp_lock *);
static __inline int __mp_release_all(struct __mp_lock *);
static __inline void __mp_acquire_count(struct __mp_lock *, int);
static __inline int __mp_lock_held(struct __mp_lock *);

/*
 * XXX Simplelocks macros used at "trusted" places.
 */
#define SIMPLELOCK		__mp_lock
#define SIMPLE_LOCK_INIT	__mp_lock_init
#define SIMPLE_LOCK		__mp_lock
#define SIMPLE_UNLOCK		__mp_unlock

static __inline void
__mp_lock_init(struct __mp_lock *lock)
{
	__cpu_simple_lock_init(&lock->mpl_lock);
	lock->mpl_cpu = LK_NOCPU;
	lock->mpl_count = 0;
}

#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

extern void Debugger(void);
extern int db_printf(const char *, ...)
    __attribute__((__format__(__kprintf__,1,2)));

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

static __inline void
__mp_lock(struct __mp_lock *lock)
{
	int s = spllock();

	if (lock->mpl_cpu != cpu_number()) {
#ifndef MP_LOCKDEBUG
		__cpu_simple_lock(&lock->mpl_lock);
#else
		{
			int got_it;
			do {
				int ticks = __mp_lock_spinout;

				do {
					got_it = __cpu_simple_lock_try(
					    &lock->mpl_lock);
				} while (!got_it && ticks-- > 0);
				if (!got_it) {
 					db_printf(
					    "__mp_lock(0x%x): lock spun out",
					    lock);
					Debugger();
				}
			} while (!got_it);
		}
#endif
		lock->mpl_cpu = cpu_number();
	}
	lock->mpl_count++;
	splx(s);
}

static __inline void
__mp_unlock(struct __mp_lock *lock)
{
	int s = spllock();

#ifdef MP_LOCKDEBUG
	if (lock->mpl_count == 0 || lock->mpl_cpu == LK_NOCPU) {
		db_printf("__mp_unlock(0x%x): releasing not locked lock\n",
		    lock);
		Debugger();
	}
#endif

	if (--lock->mpl_count == 0) {
		lock->mpl_cpu = LK_NOCPU;
		__cpu_simple_unlock(&lock->mpl_lock);
	}
	splx(s);
}

static __inline int
__mp_release_all(struct __mp_lock *lock) {
	int s = spllock();
	int rv = lock->mpl_count;

#ifdef MP_LOCKDEBUG
	if (lock->mpl_count == 0 || lock->mpl_cpu == LK_NOCPU) {
		db_printf(
		    "__mp_release_all(0x%x): releasing not locked lock\n",
		    lock);
		Debugger();
	}
#endif

	lock->mpl_cpu = LK_NOCPU;
	lock->mpl_count = 0;
	__cpu_simple_unlock(&lock->mpl_lock);
	splx(s);
	return (rv);
}

static __inline void
__mp_acquire_count(struct __mp_lock *lock, int count) {
	int s = spllock();

	__cpu_simple_lock(&lock->mpl_lock);
	lock->mpl_cpu = cpu_number();
	lock->mpl_count = count;
	splx(s);
}

static __inline int
__mp_lock_held(struct __mp_lock *lock) {
	return lock->mpl_count;
}

extern struct __mp_lock kernel_lock;

/* XXX Should really be in proc.h but then __mp_lock is not defined. */
extern struct SIMPLELOCK deadproc_slock;

#endif /* !_MPLOCK_H */

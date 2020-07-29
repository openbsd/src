/*	$OpenBSD: kern_timeout.c,v 1.72 2020/02/18 12:13:40 mpi Exp $	*/
/*
 * Copyright (c) 2001 Thomas Nordin <nordin@openbsd.org>
 * Copyright (c) 2000-2001 Artur Grabowski <art@openbsd.org>
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
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/queue.h>			/* _Q_INVALIDATE */
#include <sys/sysctl.h>
#include <sys/tracepoint.h>
#include <sys/witness.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_interface.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#endif

/*
 * Locks used to protect global variables in this file:
 *
 *	I	immutable after initialization
 *	t	timeout_mutex
 */
struct mutex timeout_mutex = MUTEX_INITIALIZER(IPL_HIGH);

void *softclock_si;			/* [I] softclock() interrupt handle */
struct timeoutstat tostat;		/* [t] statistics and totals */

/*
 * Timeouts are kept in a hierarchical timing wheel. The to_time is the value
 * of the global variable "ticks" when the timeout should be called. There are
 * four levels with 256 buckets each.
 */
#define WHEELCOUNT 4
#define WHEELSIZE 256
#define WHEELMASK 255
#define WHEELBITS 8
#define BUCKETS (WHEELCOUNT * WHEELSIZE)

struct circq timeout_wheel[BUCKETS];	/* [t] Tick-based timeouts */
struct circq timeout_wheel_hr[BUCKETS];	/* [t] High resolution timeouts */
struct circq timeout_new;		/* [t] New, unscheduled timeouts */
struct circq timeout_todo;		/* [t] Due or needs rescheduling */
struct circq timeout_proc;		/* [t] Due + needs process context */

time_t timeout_level_width[WHEELCOUNT];	/* [I] Wheel level width (seconds) */
struct timespec tick_ts;		/* [I] Length of a tick (1/hz secs) */
struct timespec timeout_lastscan;	/* [t] Uptime at last wheel scan */
struct timespec timeout_late;		/* [t] Late if due prior to this */

#define MASKWHEEL(wheel, time) (((time) >> ((wheel)*WHEELBITS)) & WHEELMASK)

#define BUCKET(rel, abs)						\
    (timeout_wheel[							\
	((rel) <= (1 << (2*WHEELBITS)))					\
	    ? ((rel) <= (1 << WHEELBITS))				\
		? MASKWHEEL(0, (abs))					\
		: MASKWHEEL(1, (abs)) + WHEELSIZE			\
	    : ((rel) <= (1 << (3*WHEELBITS)))				\
		? MASKWHEEL(2, (abs)) + 2*WHEELSIZE			\
		: MASKWHEEL(3, (abs)) + 3*WHEELSIZE])

#define MOVEBUCKET(wheel, time)						\
    CIRCQ_CONCAT(&timeout_todo,						\
        &timeout_wheel[MASKWHEEL((wheel), (time)) + (wheel)*WHEELSIZE])

/*
 * Circular queue definitions.
 */

#define CIRCQ_INIT(elem) do {			\
	(elem)->next = (elem);			\
	(elem)->prev = (elem);			\
} while (0)

#define CIRCQ_INSERT_TAIL(list, elem) do {	\
	(elem)->prev = (list)->prev;		\
	(elem)->next = (list);			\
	(list)->prev->next = (elem);		\
	(list)->prev = (elem);			\
	tostat.tos_pending++;			\
} while (0)

#define CIRCQ_CONCAT(fst, snd) do {		\
	if (!CIRCQ_EMPTY(snd)) {		\
		(fst)->prev->next = (snd)->next;\
		(snd)->next->prev = (fst)->prev;\
		(snd)->prev->next = (fst);      \
		(fst)->prev = (snd)->prev;      \
		CIRCQ_INIT(snd);		\
	}					\
} while (0)

#define CIRCQ_REMOVE(elem) do {			\
	(elem)->next->prev = (elem)->prev;      \
	(elem)->prev->next = (elem)->next;      \
	_Q_INVALIDATE((elem)->prev);		\
	_Q_INVALIDATE((elem)->next);		\
	tostat.tos_pending--;			\
} while (0)

#define CIRCQ_FIRST(elem) ((elem)->next)

#define CIRCQ_EMPTY(elem) (CIRCQ_FIRST(elem) == (elem))

#define CIRCQ_FOREACH(elem, list)		\
	for ((elem) = CIRCQ_FIRST(list);	\
	    (elem) != (list);			\
	    (elem) = CIRCQ_FIRST(elem))

#ifdef WITNESS
struct lock_object timeout_sleeplock_obj = {
	.lo_name = "timeout",
	.lo_flags = LO_WITNESS | LO_INITIALIZED | LO_SLEEPABLE |
	    (LO_CLASS_RWLOCK << LO_CLASSSHIFT)
};
struct lock_object timeout_spinlock_obj = {
	.lo_name = "timeout",
	.lo_flags = LO_WITNESS | LO_INITIALIZED |
	    (LO_CLASS_MUTEX << LO_CLASSSHIFT)
};
struct lock_type timeout_sleeplock_type = {
	.lt_name = "timeout"
};
struct lock_type timeout_spinlock_type = {
	.lt_name = "timeout"
};
#define TIMEOUT_LOCK_OBJ(needsproc) \
	((needsproc) ? &timeout_sleeplock_obj : &timeout_spinlock_obj)
#endif

void softclock(void *);
void softclock_create_thread(void *);
void softclock_thread(void *);
uint32_t timeout_bucket(struct timeout *);
int timeout_clock_is_valid(clockid_t);
int timeout_has_expired(const struct timeout *);
int timeout_is_late(const struct timeout *);
uint32_t timeout_maskwheel(uint32_t, const struct timespec *);
void timeout_reschedule(struct timeout *);
void timeout_run(struct timeout *);
void timeout_proc_barrier(void *);

int
timeout_clock_is_valid(clockid_t clock)
{
	switch (clock) {
	case CLOCK_MONOTONIC:
	case CLOCK_BOOTTIME:
		return 1;
	default:
		break;
	}
	return 0;
}

/*
 * The first thing in a struct timeout is its struct circq, so we
 * can get back from a pointer to the latter to a pointer to the
 * whole timeout with just a cast.
 */
static inline struct timeout *
timeout_from_circq(struct circq *p)
{
	return ((struct timeout *)(p));
}

static inline void
timeout_dequeue(struct timeout *to)
{
	KASSERT(ISSET(to->to_flags, TIMEOUT_ONQUEUE));
	CIRCQ_REMOVE(&to->to_list);
	CLR(to->to_flags, TIMEOUT_ONQUEUE);
}

static inline void
timeout_enqueue(struct circq *head, struct timeout *to)
{
	KASSERT(ISSET(to->to_flags, TIMEOUT_ONQUEUE) == 0);
	CIRCQ_INSERT_TAIL(head, &to->to_list);
	SET(to->to_flags, TIMEOUT_ONQUEUE);
}

static inline void
timeout_sync_order(int needsproc)
{
	WITNESS_CHECKORDER(TIMEOUT_LOCK_OBJ(needsproc), LOP_NEWORDER, NULL);
}

static inline void
timeout_sync_enter(int needsproc)
{
	timeout_sync_order(needsproc);
	WITNESS_LOCK(TIMEOUT_LOCK_OBJ(needsproc), 0);
}

static inline void
timeout_sync_leave(int needsproc)
{
	WITNESS_UNLOCK(TIMEOUT_LOCK_OBJ(needsproc), 0);
}

/*
 * Some of the "math" in here is a bit tricky.
 *
 * We have to beware of wrapping ints.
 * We use the fact that any element added to the queue must be added with a
 * positive time. That means that any element `to' on the queue cannot be
 * scheduled to timeout further in time than INT_MAX, but to->to_time can
 * be positive or negative so comparing it with anything is dangerous.
 * The only way we can use the to->to_time value in any predictable way
 * is when we calculate how far in the future `to' will timeout -
 * "to->to_time - ticks". The result will always be positive for future
 * timeouts and 0 or negative for due timeouts.
 */

void
timeout_startup(void)
{
	int b, level;

	CIRCQ_INIT(&timeout_new);
	CIRCQ_INIT(&timeout_todo);
	CIRCQ_INIT(&timeout_proc);
	for (b = 0; b < nitems(timeout_wheel); b++)
		CIRCQ_INIT(&timeout_wheel[b]);
	for (b = 0; b < nitems(timeout_wheel_hr); b++)
		CIRCQ_INIT(&timeout_wheel_hr[b]);
	for (level = 0; level < nitems(timeout_level_width); level++)
		timeout_level_width[level] = 2 << (level * WHEELBITS);
	tick_ts.tv_sec = 0;
	tick_ts.tv_nsec = tick_nsec;
	timespecclear(&timeout_lastscan);
	timespecclear(&timeout_late);
}

void
timeout_proc_init(void)
{
	softclock_si = softintr_establish(IPL_SOFTCLOCK, softclock, NULL);
	if (softclock_si == NULL)
		panic("%s: unable to register softclock interrupt", __func__);

	WITNESS_INIT(&timeout_sleeplock_obj, &timeout_sleeplock_type);
	WITNESS_INIT(&timeout_spinlock_obj, &timeout_spinlock_type);

	kthread_create_deferred(softclock_create_thread, NULL);
}

void
timeout_set(struct timeout *new, void (*fn)(void *), void *arg)
{
	timeout_set_flags(new, fn, arg, 0);
}

void
timeout_set_flags(struct timeout *to, void (*fn)(void *), void *arg, int flags)
{
	to->to_func = fn;
	to->to_arg = arg;
	to->to_flags = flags | TIMEOUT_INITIALIZED;
}

void
timeout_set_proc(struct timeout *new, void (*fn)(void *), void *arg)
{
	timeout_set_flags(new, fn, arg, TIMEOUT_PROC);
}

int
timeout_add(struct timeout *new, int to_ticks)
{
	int old_time;
	int ret = 1;

	KASSERT(ISSET(new->to_flags, TIMEOUT_INITIALIZED));
	KASSERT(to_ticks >= 0);

	mtx_enter(&timeout_mutex);

	/* Initialize the time here, it won't change. */
	old_time = new->to_time;
	new->to_time = to_ticks + ticks;
	CLR(new->to_flags, TIMEOUT_TRIGGERED | TIMEOUT_SCHEDULED);

	/*
	 * If this timeout already is scheduled and now is moved
	 * earlier, reschedule it now. Otherwise leave it in place
	 * and let it be rescheduled later.
	 */
	if (ISSET(new->to_flags, TIMEOUT_ONQUEUE)) {
		if (!ISSET(new->to_flags, TIMEOUT_TICK) ||
		    new->to_time - ticks < old_time - ticks) {
			timeout_dequeue(new);
			timeout_enqueue(&timeout_new, new);
		}
		tostat.tos_readded++;
		ret = 0;
	} else {
		timeout_enqueue(&timeout_new, new);
	}

	SET(new->to_flags, TIMEOUT_TICK);

	tostat.tos_added++;

	mtx_leave(&timeout_mutex);

	return ret;
}

int
timeout_add_tv(struct timeout *to, const struct timeval *tv)
{
	uint64_t to_ticks;

	to_ticks = (uint64_t)hz * tv->tv_sec + tv->tv_usec / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	if (to_ticks == 0 && tv->tv_usec > 0)
		to_ticks = 1;

	return timeout_add(to, (int)to_ticks);
}

int
timeout_add_ts(struct timeout *to, const struct timespec *ts)
{
	uint64_t to_ticks;

	to_ticks = (uint64_t)hz * ts->tv_sec + ts->tv_nsec / (tick * 1000);
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	if (to_ticks == 0 && ts->tv_nsec > 0)
		to_ticks = 1;

	return timeout_add(to, (int)to_ticks);
}

int
timeout_add_bt(struct timeout *to, const struct bintime *bt)
{
	uint64_t to_ticks;

	to_ticks = (uint64_t)hz * bt->sec + (long)(((uint64_t)1000000 *
	    (uint32_t)(bt->frac >> 32)) >> 32) / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	if (to_ticks == 0 && bt->frac > 0)
		to_ticks = 1;

	return timeout_add(to, (int)to_ticks);
}

int
timeout_add_sec(struct timeout *to, int secs)
{
	uint64_t to_ticks;

	to_ticks = (uint64_t)hz * secs;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return timeout_add(to, (int)to_ticks);
}

int
timeout_add_msec(struct timeout *to, int msecs)
{
	uint64_t to_ticks;

	to_ticks = (uint64_t)msecs * 1000 / tick;
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;
	if (to_ticks == 0 && msecs > 0)
		to_ticks = 1;

	return timeout_add(to, (int)to_ticks);
}

int
timeout_add_usec(struct timeout *to, int usecs)
{
	int to_ticks = usecs / tick;

	if (to_ticks == 0 && usecs > 0)
		to_ticks = 1;

	return timeout_add(to, to_ticks);
}

int
timeout_add_nsec(struct timeout *to, int nsecs)
{
	int to_ticks = nsecs / (tick * 1000);

	if (to_ticks == 0 && nsecs > 0)
		to_ticks = 1;

	return timeout_add(to, to_ticks);
}

int
timeout_at_ts(struct timeout *to, clockid_t clock, const struct timespec *ts)
{
	struct timespec old_abstime;
	int ret = 1;

	KASSERT(ISSET(to->to_flags, TIMEOUT_INITIALIZED));
	KASSERT(timeout_clock_is_valid(clock));

	mtx_enter(&timeout_mutex);

	old_abstime = to->to_abstime;
	to->to_abstime = *ts;
	CLR(to->to_flags, TIMEOUT_TRIGGERED | TIMEOUT_SCHEDULED);

	if (ISSET(to->to_flags, TIMEOUT_ONQUEUE)) {
		if (ISSET(to->to_flags, TIMEOUT_TICK) ||
		    timespeccmp(ts, &old_abstime, <)) {
			timeout_dequeue(to);
			timeout_enqueue(&timeout_new, to);
		}
		tostat.tos_readded++;
		ret = 0;
	} else {
		timeout_enqueue(&timeout_new, to);
	}

	CLR(to->to_flags, TIMEOUT_TICK);

	tostat.tos_added++;

	mtx_leave(&timeout_mutex);

	return ret;
}

int
timeout_in_nsec(struct timeout *to, uint64_t nsecs)
{
	struct timespec deadline, interval, now;

	nanouptime(&now);
	NSEC_TO_TIMESPEC(nsecs, &interval);
	timespecadd(&now, &interval, &deadline);

	return timeout_at_ts(to, CLOCK_MONOTONIC, &deadline);
}

int
timeout_advance_nsec(struct timeout *to, uint64_t nsecs, uint64_t *omissed)
{
	struct timespec intvl, next, now;
	uint64_t missed;
	int ret;

	nanouptime(&now);
	NSEC_TO_TIMESPEC(nsecs, &intvl);
	missed = itimer_advance(&to->to_abstime, &intvl, &now, &next);
	ret = timeout_at_ts(to, CLOCK_MONOTONIC, &next);
	if (omissed != NULL)
		*omissed = missed;
	return ret;
}

int
timeout_del(struct timeout *to)
{
	int ret = 0;

	mtx_enter(&timeout_mutex);
	if (ISSET(to->to_flags, TIMEOUT_ONQUEUE)) {
		timeout_dequeue(to);
		tostat.tos_cancelled++;
		ret = 1;
	}
	CLR(to->to_flags, TIMEOUT_TRIGGERED | TIMEOUT_SCHEDULED | TIMEOUT_TICK);
	tostat.tos_deleted++;
	mtx_leave(&timeout_mutex);

	return ret;
}

int
timeout_del_barrier(struct timeout *to)
{
	int removed;

	timeout_sync_order(ISSET(to->to_flags, TIMEOUT_PROC));

	removed = timeout_del(to);
	if (!removed)
		timeout_barrier(to);

	return removed;
}

void
timeout_barrier(struct timeout *to)
{
	int needsproc = ISSET(to->to_flags, TIMEOUT_PROC);

	timeout_sync_order(needsproc);

	if (!needsproc) {
		KERNEL_LOCK();
		splx(splsoftclock());
		KERNEL_UNLOCK();
	} else {
		struct cond c = COND_INITIALIZER();
		struct timeout barrier;

		timeout_set_proc(&barrier, timeout_proc_barrier, &c);

		mtx_enter(&timeout_mutex);
		timeout_enqueue(&timeout_proc, &barrier);
		mtx_leave(&timeout_mutex);

		wakeup_one(&timeout_proc);

		cond_wait(&c, "tmobar");
	}
}

void
timeout_proc_barrier(void *arg)
{
	struct cond *c = arg;

	cond_signal(c);
}

uint32_t
timeout_bucket(struct timeout *to)
{
	struct timespec diff;
	uint32_t level;

	KASSERT(!ISSET(to->to_flags, TIMEOUT_TICK));
	KASSERT(timespeccmp(&timeout_lastscan, &to->to_abstime, <));

	timespecsub(&to->to_abstime, &timeout_lastscan, &diff);
	for (level = 0; level < nitems(timeout_level_width) - 1; level++) {
		if (diff.tv_sec < timeout_level_width[level])
			break;
	}
	return level * WHEELSIZE + timeout_maskwheel(level, &to->to_abstime);
}

uint32_t
timeout_maskwheel(uint32_t level, const struct timespec *abstime)
{
	uint32_t hi, lo;

	hi = abstime->tv_sec << 7;
	lo = abstime->tv_nsec / 7812500;

	return ((hi | lo) >> (level * WHEELBITS)) & WHEELMASK;
}

/*
 * This is called from hardclock() on the primary CPU at the start of
 * every tick.
 */
void
timeout_hardclock_update(void)
{
	struct timespec elapsed, now;
	int b, done, first, last, level, need_softclock, off;

	nanouptime(&now);
	timespecsub(&now, &timeout_lastscan, &elapsed);
	need_softclock = 1;

	mtx_enter(&timeout_mutex);

	MOVEBUCKET(0, ticks);
	if (MASKWHEEL(0, ticks) == 0) {
		MOVEBUCKET(1, ticks);
		if (MASKWHEEL(1, ticks) == 0) {
			MOVEBUCKET(2, ticks);
			if (MASKWHEEL(2, ticks) == 0)
				MOVEBUCKET(3, ticks);
		}
	}

	/*
	 * Dump the buckets that expired while we were away.
	 *
	 * If the elapsed time has exceeded a level's limit then we need
	 * to dump every bucket in the level.  We have necessarily completed
	 * a lap of that level, too, so we need to process buckets in the
	 * next level.
	 *
	 * Otherwise we need to compare indices: if the index of the first
	 * expired bucket is greater than that of the last then we have
	 * completed a lap of the level and need to process buckets in the
	 * next level.
	 */
	for (level = 0; level < nitems(timeout_level_width); level++) {
		first = timeout_maskwheel(level, &timeout_lastscan);
		if (elapsed.tv_sec >= timeout_level_width[level]) {
			last = (first == 0) ? WHEELSIZE - 1 : first - 1;
			done = 0;
		} else {
			last = timeout_maskwheel(level, &now);
			done = first <= last;
		}
		off = level * WHEELSIZE;
		for (b = first;; b = (b + 1) % WHEELSIZE) {
			CIRCQ_CONCAT(&timeout_todo, &timeout_wheel_hr[off + b]);
			if (b == last)
				break;
		}
		if (done)
			break;
	}

	timeout_lastscan = now;
	timespecsub(&timeout_lastscan, &tick_ts, &timeout_late);

	if (CIRCQ_EMPTY(&timeout_todo) && CIRCQ_EMPTY(&timeout_new))
		need_softclock = 0;

	mtx_leave(&timeout_mutex);

	if (need_softclock)
		softintr_schedule(softclock_si);
}

void
timeout_run(struct timeout *to)
{
	void (*fn)(void *);
	void *arg;
	int needsproc;

	MUTEX_ASSERT_LOCKED(&timeout_mutex);

	CLR(to->to_flags, TIMEOUT_SCHEDULED | TIMEOUT_TICK);
	SET(to->to_flags, TIMEOUT_TRIGGERED);

	fn = to->to_func;
	arg = to->to_arg;
	needsproc = ISSET(to->to_flags, TIMEOUT_PROC);

	mtx_leave(&timeout_mutex);
	timeout_sync_enter(needsproc);
	fn(arg);
	timeout_sync_leave(needsproc);
	mtx_enter(&timeout_mutex);
}

int
timeout_has_expired(const struct timeout *to)
{
	if (ISSET(to->to_flags, TIMEOUT_TICK))
		return (to->to_time - ticks) <= 0;
	return timespeccmp(&to->to_abstime, &timeout_lastscan, <=);
}

int
timeout_is_late(const struct timeout *to)
{
	if (!ISSET(to->to_flags, TIMEOUT_SCHEDULED))
		return 0;

	if (ISSET(to->to_flags, TIMEOUT_TICK))
		return (to->to_time - ticks) < 0;
	return timespeccmp(&to->to_abstime, &timeout_late, <=);
}

void
timeout_reschedule(struct timeout *to)
{
	if (ISSET(to->to_flags, TIMEOUT_SCHEDULED))
		tostat.tos_rescheduled++;
	else
		SET(to->to_flags, TIMEOUT_SCHEDULED);
	tostat.tos_scheduled++;

	if (ISSET(to->to_flags, TIMEOUT_TICK))
		timeout_enqueue(&BUCKET(to->to_time - ticks, to->to_time), to);
	else
		timeout_enqueue(&timeout_wheel_hr[timeout_bucket(to)], to);
}

/*
 * Timeouts are processed here instead of timeout_hardclock_update()
 * to avoid doing any more work at IPL_CLOCK than absolutely necessary.
 * Down here at IPL_SOFTCLOCK other interrupts can be serviced promptly
 * so the system remains responsive even if there is a surge of timeouts.
 */
void
softclock(void *arg)
{
	struct timeout *to;
	int needsproc;

#ifdef TIMEOUT_DEBUG
	struct timespec begin, end;
	nanouptime(&begin);
#endif

	mtx_enter(&timeout_mutex);
	CIRCQ_CONCAT(&timeout_todo, &timeout_new);
	while (!CIRCQ_EMPTY(&timeout_todo)) {
		to = timeout_from_circq(CIRCQ_FIRST(&timeout_todo));
		timeout_dequeue(to);
		if (!timeout_has_expired(to)) {
			timeout_reschedule(to);
			continue;
		}
		if (timeout_is_late(to))
			tostat.tos_late++;			
		if (ISSET(to->to_flags, TIMEOUT_PROC)) {
			timeout_enqueue(&timeout_proc, to);
			continue;
		}
		timeout_run(to);
		tostat.tos_run_softclock++;
	}
	tostat.tos_softclocks++;
	needsproc = !CIRCQ_EMPTY(&timeout_proc);
	mtx_leave(&timeout_mutex);

#ifdef TIMEOUT_DEBUG
	nanouptime(&end);
	TRACEPOINT(timeout, softclock,
	    TIMESPEC_TO_NSEC(&end) - TIMESPEC_TO_NSEC(&begin));
#endif

	if (needsproc)
		wakeup(&timeout_proc);
}

void
softclock_create_thread(void *arg)
{
	if (kthread_create(softclock_thread, NULL, NULL, "softclock"))
		panic("fork softclock");
}

void
softclock_thread(void *arg)
{
	CPU_INFO_ITERATOR cii;
	struct cpu_info *ci;
	struct sleep_state sls;
	struct timeout *to;
	int s;

	KERNEL_ASSERT_LOCKED();

	/* Be conservative for the moment */
	CPU_INFO_FOREACH(cii, ci) {
		if (CPU_IS_PRIMARY(ci))
			break;
	}
	KASSERT(ci != NULL);
	sched_peg_curproc(ci);

	s = splsoftclock();
	for (;;) {
		sleep_setup(&sls, &timeout_proc, PSWP, "bored");
		sleep_finish(&sls, CIRCQ_EMPTY(&timeout_proc));

		mtx_enter(&timeout_mutex);
		while (!CIRCQ_EMPTY(&timeout_proc)) {
			to = timeout_from_circq(CIRCQ_FIRST(&timeout_proc));
			timeout_dequeue(to);
			timeout_run(to);
			tostat.tos_run_thread++;
		}
		tostat.tos_thread_wakeups++;
		mtx_leave(&timeout_mutex);
	}
	splx(s);
}

#ifndef SMALL_KERNEL
void
timeout_adjust_ticks(int adj)
{
	struct timeout *to;
	struct circq *p;
	int new_ticks, b;

	/* adjusting the monotonic clock backwards would be a Bad Thing */
	if (adj <= 0)
		return;

	mtx_enter(&timeout_mutex);
	new_ticks = ticks + adj;
	for (b = 0; b < nitems(timeout_wheel); b++) {
		p = CIRCQ_FIRST(&timeout_wheel[b]);
		while (p != &timeout_wheel[b]) {
			to = timeout_from_circq(p);
			p = CIRCQ_FIRST(p);

			/* when moving a timeout forward need to reinsert it */
			if (to->to_time - ticks < adj)
				to->to_time = new_ticks;
			timeout_dequeue(to);
			timeout_enqueue(&timeout_todo, to);
		}
	}
	ticks = new_ticks;
	mtx_leave(&timeout_mutex);
}
#endif

int
timeout_sysctl(void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	struct timeoutstat status;

	mtx_enter(&timeout_mutex);
	memcpy(&status, &tostat, sizeof(status));
	mtx_leave(&timeout_mutex);

	return sysctl_rdstruct(oldp, oldlenp, newp, &status, sizeof(status));
}

#ifdef DDB
void db_show_callout_bucket(struct circq *);
void db_show_timeout(struct timeout *, struct circq *);
char *db_strtimespec(const struct timespec *);

void
db_show_callout_bucket(struct circq *bucket)
{
	struct circq *p;

	CIRCQ_FOREACH(p, bucket)
		db_show_timeout(timeout_from_circq(p), bucket);
}

void
db_show_timeout(struct timeout *to, struct circq *bucket)
{
	struct timespec remaining;
	char buf[8];
	db_expr_t offset;
	struct circq *wheel;
	char *name, *where;
	int width = sizeof(long) * 2;

	db_find_sym_and_offset((vaddr_t)to->to_func, &name, &offset);
	name = name ? name : "?";
	if (bucket == &timeout_new)
		where = "new";
	else if (bucket == &timeout_todo)
		where = "softint";
	else if (bucket == &timeout_proc)
		where = "thread";
	else {
		if (ISSET(to->to_flags, TIMEOUT_TICK))
			wheel = timeout_wheel;
		else
			wheel = timeout_wheel_hr;
		snprintf(buf, sizeof(buf), "%3ld/%1ld",
		    (bucket - wheel) % WHEELSIZE,
		    (bucket - wheel) / WHEELSIZE);
		where = buf;
	}
	if (ISSET(to->to_flags, TIMEOUT_TICK)) {
		db_printf("%20d  %5s  %7s  0x%0*lx  %s\n",
		    to->to_time - ticks, "ticks", where,
		    width, (ulong)to->to_arg, name);
	} else {
		timespecsub(&to->to_abstime, &timeout_lastscan, &remaining);
		db_printf("%20s  %5s  %7s  0x%0*lx  %s\n",
		    db_strtimespec(&remaining), "mono", where,
		    width, (ulong)to->to_arg, name);
	}
}

char *
db_strtimespec(const struct timespec *ts)
{
	static char buf[32];
	struct timespec tmp, zero;

	if (ts->tv_sec >= 0) {
		snprintf(buf, sizeof(buf), "%lld.%09ld",
		    ts->tv_sec, ts->tv_nsec);
		return buf;
	}

	timespecclear(&zero);
	timespecsub(&zero, ts, &tmp);
	snprintf(buf, sizeof(buf), "-%lld.%09ld", tmp.tv_sec, tmp.tv_nsec);
	return buf;
}

void
db_show_callout(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	int width = sizeof(long) * 2 + 2;
	int b;

	db_printf("%20s  %5s\n", "last-scan-time", "clock");
	db_printf("%20d  %5s\n", ticks, "ticks");
	db_printf("%20s  %5s\n", db_strtimespec(&timeout_lastscan), "mono");
	db_printf("\n");	
	db_printf("%20s  %5s  %7s  %*s  func\n",
	    "remaining", "clock", "wheel", width, "arg");
	db_show_callout_bucket(&timeout_new);
	db_show_callout_bucket(&timeout_todo);
	db_show_callout_bucket(&timeout_proc);
	for (b = 0; b < nitems(timeout_wheel); b++)
		db_show_callout_bucket(&timeout_wheel[b]);
}
#endif

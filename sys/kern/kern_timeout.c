/*	$OpenBSD: kern_timeout.c,v 1.64 2019/11/29 12:50:48 cheloha Exp $	*/
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
#include <sys/timeout.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/queue.h>			/* _Q_INVALIDATE */
#include <sys/sysctl.h>
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
 * of the system uptime clock when the timeout should be called. There are
 * four levels with 256 buckets each.
 */
#define WHEELCOUNT 4
#define WHEELSIZE 256
#define WHEELMASK 255
#define WHEELBITS 8
#define BUCKETS (WHEELCOUNT * WHEELSIZE)

struct circq timeout_wheel[BUCKETS];	/* [t] Queues of timeouts */
struct circq timeout_todo;		/* [t] Due or needs scheduling */
struct circq timeout_proc;		/* [t] Due + needs process context */

time_t timeout_level_width[WHEELCOUNT];	/* [I] Wheel level width (seconds) */
struct timespec tick_ts;		/* [I] Length of a tick (1/hz secs) */
struct timespec timeout_lastscan;	/* [t] Uptime at last wheel scan */
struct timespec timeout_late;		/* [t] Late if due prior to this */

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
		(snd)->prev->next = (fst);	\
		(fst)->prev = (snd)->prev;	\
		CIRCQ_INIT(snd);		\
	}					\
} while (0)

#define CIRCQ_REMOVE(elem) do {			\
	(elem)->next->prev = (elem)->prev;	\
	(elem)->prev->next = (elem)->next;	\
	_Q_INVALIDATE((elem)->prev);		\
	_Q_INVALIDATE((elem)->next);		\
	tostat.tos_pending--;			\
} while (0)

#define CIRCQ_FIRST(elem) ((elem)->next)

#define CIRCQ_EMPTY(elem) (CIRCQ_FIRST(elem) == (elem))

#define CIRCQ_FOREACH(elem, head)		\
	for ((elem) = CIRCQ_FIRST(head);	\
	    (elem) != (head);			\
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
int timeout_at_ts_locked(struct timeout *, clockid_t, const struct timespec *);
unsigned int timeout_bucket(const struct timespec *, const struct timespec *);
int timeout_clock_is_valid(clockid_t);
unsigned int timeout_maskwheel(unsigned int, const struct timespec *);
void timeout_proc_barrier(void *);

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

void
timeout_startup(void)
{
	unsigned int b, level;

	CIRCQ_INIT(&timeout_todo);
	CIRCQ_INIT(&timeout_proc);
	for (b = 0; b < nitems(timeout_wheel); b++)
		CIRCQ_INIT(&timeout_wheel[b]);

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
	new->to_func = fn;
	new->to_arg = arg;
	new->to_flags = TIMEOUT_INITIALIZED;
	timespecclear(&new->to_time);
}

void
timeout_set_proc(struct timeout *new, void (*fn)(void *), void *arg)
{
	timeout_set(new, fn, arg);
	SET(new->to_flags, TIMEOUT_NEEDPROCCTX);
}

int
timeout_add(struct timeout *new, int to_ticks)
{
	struct timespec ts, when;
	int ret;

	KASSERT(to_ticks >= 0);
	NSEC_TO_TIMESPEC((uint64_t)to_ticks * tick_nsec, &ts);

	mtx_enter(&timeout_mutex);
	timespecadd(&timeout_lastscan, &ts, &when);
	ret = timeout_at_ts_locked(new, CLOCK_BOOTTIME, &when);
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
	int ret;

	mtx_enter(&timeout_mutex);
	ret = timeout_at_ts_locked(to, clock, ts);
	mtx_leave(&timeout_mutex);

	return ret;
}

int
timeout_at_ts_locked(struct timeout *to, clockid_t clock,
    const struct timespec *when)
{
	struct timespec old_time;
	int ret = 1;

	MUTEX_ASSERT_LOCKED(&timeout_mutex);
	KASSERT(ISSET(to->to_flags, TIMEOUT_INITIALIZED));
	KASSERT(timeout_clock_is_valid(clock));

	old_time = to->to_time;
	to->to_time = *when;
	CLR(to->to_flags, TIMEOUT_TRIGGERED);

	if (ISSET(to->to_flags, TIMEOUT_ONQUEUE)) {
		if (timespeccmp(&to->to_time, &old_time, <)) {
			CIRCQ_REMOVE(&to->to_list);
			CIRCQ_INSERT_TAIL(&timeout_todo, &to->to_list);
		}
		tostat.tos_readded++;
		ret = 0;
	} else {
		SET(to->to_flags, TIMEOUT_ONQUEUE);
		CIRCQ_INSERT_TAIL(&timeout_todo, &to->to_list);
	}
	tostat.tos_added++;

	return ret;
}

int
timeout_del(struct timeout *to)
{
	int ret = 0;

	mtx_enter(&timeout_mutex);
	if (ISSET(to->to_flags, TIMEOUT_ONQUEUE)) {
		CIRCQ_REMOVE(&to->to_list);
		CLR(to->to_flags, TIMEOUT_ONQUEUE);
		tostat.tos_cancelled++;
		ret = 1;
	}
	CLR(to->to_flags, TIMEOUT_TRIGGERED);
	tostat.tos_deleted++;
	mtx_leave(&timeout_mutex);

	return ret;
}

int
timeout_del_barrier(struct timeout *to)
{
	int removed;

	timeout_sync_order(ISSET(to->to_flags, TIMEOUT_NEEDPROCCTX));

	removed = timeout_del(to);
	if (!removed)
		timeout_barrier(to);

	return removed;
}

void
timeout_barrier(struct timeout *to)
{
	int needsproc = ISSET(to->to_flags, TIMEOUT_NEEDPROCCTX);

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
		SET(barrier.to_flags, TIMEOUT_ONQUEUE);
		CIRCQ_INSERT_TAIL(&timeout_proc, &barrier.to_list);
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

unsigned int
timeout_maskwheel(unsigned int level, const struct timespec *abstime)
{
	uint32_t hi, lo;

	hi = abstime->tv_sec << 7;
	lo = abstime->tv_nsec / 7812500;

	return ((hi | lo) >> (level * WHEELBITS)) & WHEELMASK;
}

unsigned int
timeout_bucket(const struct timespec *now, const struct timespec *later)
{
	struct timespec diff;
	unsigned int level;

	KASSERT(timespeccmp(now, later, <));

	timespecsub(later, now, &diff);
	for (level = 0; level < nitems(timeout_level_width) - 1; level++) {
		if (diff.tv_sec < timeout_level_width[level])
			break;
	}
	return level * WHEELSIZE + timeout_maskwheel(level, later);
}

int
timeout_clock_is_valid(clockid_t clock)
{
	switch (clock) {
	case CLOCK_BOOTTIME:
	case CLOCK_MONOTONIC:
		return 1;
	default:
		break;
	}
	return 0;
}

/*
 * This is called from hardclock() on the primary CPU at the start of
 * every tick.
 */
void
timeout_hardclock_update(void)
{
	struct timespec elapsed, now;
	unsigned int b, done, first, last, level, need_softclock, offset;

	nanouptime(&now);

	mtx_enter(&timeout_mutex);

	/*
	 * Dump the buckets that expired while we were away.
	 *
	 * If the elapsed time has exceeded a level's width then we need
	 * to dump every bucket in the level.  We have necessarily completed
	 * a lap of that level so we need to process buckets in the next.
	 *
	 * Otherwise we just need to compare indices.  If the index of the
	 * first expired bucket is greater than or equal to that of the last
	 * then we have completed a lap of the level and need to process
	 * buckets in the next.
	 */
	timespecsub(&now, &timeout_lastscan, &elapsed);
	for (level = 0; level < nitems(timeout_level_width); level++) {
		first = timeout_maskwheel(level, &timeout_lastscan);
		if (elapsed.tv_sec >= timeout_level_width[level]) {
			last = (first == 0) ? WHEELSIZE - 1 : first - 1;
			done = 0;
		} else {
			last = timeout_maskwheel(level, &now);
			done = first <= last;
		}
		offset = level * WHEELSIZE;
		for (b = first;; b = (b + 1) % WHEELSIZE) {
			CIRCQ_CONCAT(&timeout_todo, &timeout_wheel[offset + b]);
			if (b == last)
				break;
		}
		if (done)
			break;
	}

	timespecsub(&now, &tick_ts, &timeout_late);
	timeout_lastscan = now;
	need_softclock = !CIRCQ_EMPTY(&timeout_todo);

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

	CLR(to->to_flags, TIMEOUT_ONQUEUE);
	SET(to->to_flags, TIMEOUT_TRIGGERED);

	fn = to->to_func;
	arg = to->to_arg;
	needsproc = ISSET(to->to_flags, TIMEOUT_NEEDPROCCTX);

	mtx_leave(&timeout_mutex);
	timeout_sync_enter(needsproc);
	fn(arg);
	timeout_sync_leave(needsproc);
	mtx_enter(&timeout_mutex);
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
	unsigned int b, needsproc = 0;

	mtx_enter(&timeout_mutex);
	while (!CIRCQ_EMPTY(&timeout_todo)) {
		to = timeout_from_circq(CIRCQ_FIRST(&timeout_todo));
		CIRCQ_REMOVE(&to->to_list);

		/*
		 * If due run it or defer execution to the thread,
		 * otherwise insert it into the right bucket.
		 */
		if (timespeccmp(&timeout_lastscan, &to->to_time, <)) {
			b = timeout_bucket(&timeout_lastscan, &to->to_time);
			CIRCQ_INSERT_TAIL(&timeout_wheel[b], &to->to_list);
			tostat.tos_rescheduled++;
			continue;
		}
		if (timespeccmp(&to->to_time, &timeout_late, <))
			tostat.tos_late++;
		if (ISSET(to->to_flags, TIMEOUT_NEEDPROCCTX)) {
			CIRCQ_INSERT_TAIL(&timeout_proc, &to->to_list);
			needsproc = 1;
			continue;
		}
		timeout_run(to);
		tostat.tos_run_softclock++;
	}
	tostat.tos_softclocks++;
	mtx_leave(&timeout_mutex);

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

	KERNEL_ASSERT_LOCKED();

	/* Be conservative for the moment */
	CPU_INFO_FOREACH(cii, ci) {
		if (CPU_IS_PRIMARY(ci))
			break;
	}
	KASSERT(ci != NULL);
	sched_peg_curproc(ci);

	for (;;) {
		sleep_setup(&sls, &timeout_proc, PSWP, "bored");
		sleep_finish(&sls, CIRCQ_EMPTY(&timeout_proc));

		mtx_enter(&timeout_mutex);
		while (!CIRCQ_EMPTY(&timeout_proc)) {
			to = timeout_from_circq(CIRCQ_FIRST(&timeout_proc));
			CIRCQ_REMOVE(&to->to_list);
			timeout_run(to);
			tostat.tos_run_thread++;
		}
		tostat.tos_thread_wakeups++;
		mtx_leave(&timeout_mutex);
	}
}

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
char *db_ts_str(const struct timespec *);

char *
db_ts_str(const struct timespec *ts)
{
	static char buf[64];
	struct timespec tmp = *ts;
	int neg = 0;

	if (tmp.tv_sec < 0) {
		tmp.tv_sec = -tmp.tv_sec;
		if (tmp.tv_nsec > 0) {
			tmp.tv_sec--;
			tmp.tv_nsec = 1000000000 - tmp.tv_nsec;
		}
		neg = 1;
	}

	snprintf(buf, sizeof(buf), "%s%lld.%09ld",
	    neg ? "-" : "", tmp.tv_sec, tmp.tv_nsec);

	return buf;
}

void
db_show_callout_bucket(struct circq *bucket)
{
	struct timespec left;
	char buf[8];
	struct timeout *to;
	struct circq *p;
	db_expr_t offset;
	char *name, *where;
	int width = sizeof(long) * 2;

	CIRCQ_FOREACH(p, bucket) {
		to = timeout_from_circq(p);
		db_find_sym_and_offset((vaddr_t)to->to_func, &name, &offset);
		name = name ? name : "?";
		if (bucket == &timeout_todo)
			where = "softint";
		else if (bucket == &timeout_proc)
			where = "thread";
		else {
			snprintf(buf, sizeof(buf), "%3ld/%1ld",
			    (bucket - timeout_wheel) % WHEELSIZE,
			    (bucket - timeout_wheel) / WHEELSIZE);
			where = buf;
		}
		timespecsub(&to->to_time, &timeout_lastscan, &left);
		db_printf("%18s  %7s  0x%0*lx  %s\n",
		    db_ts_str(&left), where, width, (ulong)to->to_arg, name);
	}
}

void
db_show_callout(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	int width = sizeof(long) * 2 + 2;
	unsigned int b;

	db_printf("%18s seconds up at last scan\n",
	    db_ts_str(&timeout_lastscan));
	db_printf("%18s  %7s  %*s  func\n", "remaining", "wheel", width, "arg");

	db_show_callout_bucket(&timeout_todo);
	db_show_callout_bucket(&timeout_proc);
	for (b = 0; b < nitems(timeout_wheel); b++)
		db_show_callout_bucket(&timeout_wheel[b]);
}
#endif

/* $OpenBSD: kern_clockintr.c,v 1.1 2022/11/05 19:29:46 cheloha Exp $ */
/*
 * Copyright (c) 2003 Dale Rahn <drahn@openbsd.org>
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2020-2022 Scott Cheloha <cheloha@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/clockintr.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/stdint.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#ifdef __HAVE_CLOCKINTR

/*
 * Protection for global variables in this file:
 *
 *	C	Global clockintr configuration mutex (clockintr_mtx).
 *	I	Immutable after initialization.
 */
struct mutex clockintr_mtx = MUTEX_INITIALIZER(IPL_CLOCK);

u_int clockintr_flags;			/* [I] global state + behavior flags */
uint32_t hardclock_period;		/* [I] hardclock period (ns) */
uint32_t schedclock_period;		/* [I] schedclock period (ns) */
volatile u_int statclock_gen = 1;	/* [C] statclock update generation */
volatile uint32_t statclock_avg;	/* [C] average statclock period (ns) */
uint32_t statclock_min;			/* [C] minimum statclock period (ns) */
uint32_t statclock_mask;		/* [C] set of allowed offsets */
uint32_t stat_avg;			/* [I] average stathz period (ns) */
uint32_t stat_min;			/* [I] set of allowed offsets */
uint32_t stat_mask;			/* [I] max offset from minimum (ns) */
uint32_t prof_avg;			/* [I] average profhz period (ns) */
uint32_t prof_min;			/* [I] minimum profhz period (ns) */
uint32_t prof_mask;			/* [I] set of allowed offsets */

void clockintr_statvar_init(int, uint32_t *, uint32_t *, uint32_t *);
uint64_t nsec_advance(uint64_t *, uint64_t, uint64_t);

/*
 * Initialize global state.  Set flags and compute intervals.
 */
void
clockintr_init(u_int flags)
{
	KASSERT(CPU_IS_PRIMARY(curcpu()));
	KASSERT(clockintr_flags == 0);
	KASSERT(!ISSET(flags, ~CL_FLAG_MASK));

	KASSERT(hz > 0 && hz <= 1000000000);
	hardclock_period = 1000000000 / hz;

	KASSERT(stathz >= 1 && stathz <= 1000000000);
	KASSERT(profhz >= stathz && profhz <= 1000000000);
	KASSERT(profhz % stathz == 0);
	clockintr_statvar_init(stathz, &stat_avg, &stat_min, &stat_mask);
	clockintr_statvar_init(profhz, &prof_avg, &prof_min, &prof_mask);
	SET(clockintr_flags, CL_STATCLOCK);
	clockintr_setstatclockrate(stathz);

	KASSERT(schedhz >= 0 && schedhz <= 1000000000);
	if (schedhz != 0) {
		schedclock_period = 1000000000 / schedhz;
		SET(clockintr_flags, CL_SCHEDCLOCK);
	}

	SET(clockintr_flags, flags | CL_INIT);
}

/*
 * Ready the calling CPU for clockintr_dispatch().  If this is our
 * first time here, install the intrclock, if any, and set necessary
 * flags.  Advance the schedule as needed.
 */
void
clockintr_cpu_init(const struct intrclock *ic)
{
	uint64_t multiplier, now;
	struct cpu_info *ci = curcpu();
	struct clockintr_queue *cq = &ci->ci_queue;

	KASSERT(ISSET(clockintr_flags, CL_INIT));

	if (!ISSET(cq->cq_flags, CL_CPU_INIT)) {
		if (ic != NULL) {
			cq->cq_intrclock = *ic;
			SET(cq->cq_flags, CL_CPU_INTRCLOCK);
		}
		cq->cq_gen = 1;
	}

	/*
	 * Until we understand scheduler lock contention better, stagger
	 * the hardclock and statclock so they don't all happen at once.
	 * If we have no intrclock it doesn't matter, we have no control
	 * anyway.  The primary CPU's starting offset is always zero, so
	 * set multiplier to zero.
	 */
	if (!CPU_IS_PRIMARY(ci) && ISSET(cq->cq_flags, CL_CPU_INTRCLOCK))
		multiplier = CPU_INFO_UNIT(ci);
	else
		multiplier = 0;

	now = nsecuptime();

	/*
	 * The first time we do this, the primary CPU cannot skip any
	 * hardclocks.  We can skip hardclocks on subsequent calls because
	 * the global tick value is advanced during inittodr(9) on our
	 * behalf.
	 */
	if (!CPU_IS_PRIMARY(ci) || ISSET(cq->cq_flags, CL_CPU_INIT)) {
		cq->cq_next_hardclock = hardclock_period / ncpus * multiplier;
		nsec_advance(&cq->cq_next_hardclock, hardclock_period, now);
	}

	/*
	 * We can always advance the statclock and schedclock.
	 */
	cq->cq_next_statclock = stat_avg / ncpus * multiplier;
	nsec_advance(&cq->cq_next_statclock, stat_avg, now);
	if (ISSET(clockintr_flags, CL_SCHEDCLOCK)) {
		cq->cq_next_schedclock = schedclock_period / ncpus * multiplier;
		nsec_advance(&cq->cq_next_schedclock, schedclock_period, now);
	}

	SET(cq->cq_flags, CL_CPU_INIT);
}

/*
 * If we have an intrclock, trigger it to start the dispatch cycle.
 */
void
clockintr_trigger(void)
{
	struct clockintr_queue *cq = &curcpu()->ci_queue;

	KASSERT(ISSET(cq->cq_flags, CL_CPU_INIT));

	if (ISSET(cq->cq_flags, CL_CPU_INTRCLOCK))
		intrclock_trigger(&cq->cq_intrclock);
}

/*
 * Run all expired events scheduled on the calling CPU.
 */
int
clockintr_dispatch(void *frame)
{
	uint64_t count, i, lateness, now, run = 0, start;
	struct cpu_info *ci = curcpu();
	struct clockintr_queue *cq = &ci->ci_queue;
	struct proc *p = curproc;
	uint32_t mask, min, off;
	u_int gen, ogen;

	if (cq->cq_dispatch != 0)
		panic("%s: recursive dispatch", __func__);
	cq->cq_dispatch = 1;

	splassert(IPL_CLOCK);
	KASSERT(ISSET(cq->cq_flags, CL_CPU_INIT));

	/*
	 * If we arrived too early we have nothing to do.
	 */
	start = nsecuptime();
	now = start;
	if (now < cq->cq_next)
		goto done;
	lateness = now - cq->cq_next;

	/*
	 * Dispatch expired events.
	 */
again:
	/* hardclock */
	count = nsec_advance(&cq->cq_next_hardclock, hardclock_period, now);
	for (i = 0; i < count; i++)
		hardclock(frame);
	run += count;

	/* statclock */
	if (ISSET(clockintr_flags, CL_RNDSTAT)) {
		do {
			gen = statclock_gen;
			membar_consumer();
			min = statclock_min;
			mask = statclock_mask;
			membar_consumer();
		} while (gen == 0 || gen != statclock_gen);
		count = 0;
		while (cq->cq_next_statclock <= now) {
			while ((off = (random() & mask)) == 0)
				continue;
			cq->cq_next_statclock += min + off;
			count++;
		}
	} else {
		count = nsec_advance(&cq->cq_next_statclock, statclock_avg,
		    now);
	}
	for (i = 0; i < count; i++)
		statclock(frame);
	run += count;

	/* schedclock */
	if (ISSET(clockintr_flags, CL_SCHEDCLOCK)) {
		count = nsec_advance(&cq->cq_next_schedclock,
		    schedclock_period, now);
		if (p != NULL) {
			for (i = 0; i < count; i++)
				schedclock(p);
		}
		run += count;
	}

	/* Run the dispatch again if the next event has already expired. */
	cq->cq_next = cq->cq_next_hardclock;
	if (cq->cq_next_statclock < cq->cq_next)
		cq->cq_next = cq->cq_next_statclock;
	if (ISSET(clockintr_flags, CL_SCHEDCLOCK)) {
		if (cq->cq_next_schedclock < cq->cq_next)
			cq->cq_next = cq->cq_next_schedclock;
	}
	now = nsecuptime();
	if (cq->cq_next <= now)
		goto again;

	/*
	 * Dispatch complete.
	 */
done:
	/* Rearm the interrupt clock if we have one. */
	if (ISSET(cq->cq_flags, CL_CPU_INTRCLOCK))
		intrclock_rearm(&cq->cq_intrclock, cq->cq_next - now);

	/* Update our stats. */
	ogen = cq->cq_gen;
	cq->cq_gen = 0;
	membar_producer();
	cq->cq_stat.cs_dispatched += now - start;
	if (run > 0) {
		cq->cq_stat.cs_lateness += lateness;
		cq->cq_stat.cs_prompt++;
		cq->cq_stat.cs_run += run;
	} else {
		cq->cq_stat.cs_early++;
		cq->cq_stat.cs_earliness += cq->cq_next - now;
	}
	membar_producer();
	cq->cq_gen = MAX(1, ogen + 1);

	if (cq->cq_dispatch != 1)
		panic("%s: unexpected value: %u", __func__, cq->cq_dispatch);
	cq->cq_dispatch = 0;

	return run > 0;
}

/*
 * Compute the period (avg) for the given frequency and a range around
 * that period.  The range is [min + 1, min + mask].  The range is used
 * during dispatch to choose a new pseudorandom deadline for each statclock
 * event.
 */
void
clockintr_statvar_init(int freq, uint32_t *avg, uint32_t *min, uint32_t *mask)
{
	uint32_t half_avg, var;

	KASSERT(!ISSET(clockintr_flags, CL_INIT | CL_STATCLOCK));
	KASSERT(freq > 0 && freq <= 1000000000);

	/* Compute avg, the average period. */
	*avg = 1000000000 / freq;

	/* Find var, the largest power of two such that var <= avg / 2. */
	half_avg = *avg / 2;
	for (var = 1U << 31; var > half_avg; var /= 2)
		continue;

	/* Using avg and var, set a lower bound for the range. */
	*min = *avg - (var / 2);

	/* The mask is just (var - 1). */
	*mask = var - 1;
}

/*
 * Update the statclock_* variables according to the given frequency.
 * Must only be called after clockintr_statvar_init() initializes both
 * stathz_* and profhz_*.
 */
void
clockintr_setstatclockrate(int freq)
{
	u_int ogen;

	KASSERT(ISSET(clockintr_flags, CL_STATCLOCK));

	mtx_enter(&clockintr_mtx);

	ogen = statclock_gen;
	statclock_gen = 0;
	membar_producer();
	if (freq == stathz) {
		statclock_avg = stat_avg;
		statclock_min = stat_min;
		statclock_mask = stat_mask;
	} else if (freq == profhz) {
		statclock_avg = prof_avg;
		statclock_min = prof_min;
		statclock_mask = prof_mask;
	} else {
		panic("%s: frequency is not stathz (%d) or profhz (%d): %d",
		    __func__, stathz, profhz, freq);
	}
	membar_producer();
	statclock_gen = MAX(1, ogen + 1);

	mtx_leave(&clockintr_mtx);
}

/*
 * Advance *next in increments of period until it exceeds now.
 * Returns the number of increments *next was advanced.
 *
 * We check the common cases first to avoid division if possible.
 * This does no overflow checking.
 */
uint64_t
nsec_advance(uint64_t *next, uint64_t period, uint64_t now)
{
	uint64_t elapsed;

	if (now < *next)
		return 0;

	if (now < *next + period) {
		*next += period;
		return 1;
	}

	elapsed = (now - *next) / period + 1;
	*next += period * elapsed;
	return elapsed;
}

int
sysctl_clockintr(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	struct clockintr_stat sum = { 0 }, tmp;
	struct clockintr_queue *cq;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
	u_int gen;

	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {
	case KERN_CLOCKINTR_STATS:
		CPU_INFO_FOREACH(cii, ci) {
			cq = &ci->ci_queue;
			if (!ISSET(cq->cq_flags, CL_CPU_INIT))
				continue;
			do {
				gen = cq->cq_gen;
				membar_consumer();
				tmp = cq->cq_stat;
				membar_consumer();
			} while (gen == 0 || gen != cq->cq_gen);
			sum.cs_dispatched += tmp.cs_dispatched;
			sum.cs_early += tmp.cs_early;
			sum.cs_earliness += tmp.cs_earliness;
			sum.cs_lateness += tmp.cs_lateness;
			sum.cs_prompt += tmp.cs_prompt;
			sum.cs_run += tmp.cs_run;
		}
		return sysctl_rdstruct(oldp, oldlenp, newp, &sum, sizeof sum);
	default:
		break;
	}

	return EINVAL;
}

#ifdef DDB

#include <machine/db_machdep.h>

#include <ddb/db_interface.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>

void db_show_clockintr(uint64_t, u_int, const char *);
void db_show_clockintr_cpu(struct cpu_info *);

void
db_show_all_clockintr(db_expr_t addr, int haddr, db_expr_t count, char *modif)
{
	struct timespec now;
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;

	nanouptime(&now);
	db_printf("%20s\n", "UPTIME");
	db_printf("%10lld.%09ld\n", now.tv_sec, now.tv_nsec);
	db_printf("\n");
	db_printf("%20s  %3s  %s\n", "EXPIRATION", "CPU", "NAME");
	CPU_INFO_FOREACH(cii, ci) {
		if (ISSET(ci->ci_queue.cq_flags, CL_CPU_INIT))
			db_show_clockintr_cpu(ci);
	}
}

void
db_show_clockintr_cpu(struct cpu_info *ci)
{
	struct clockintr_queue *cq = &ci->ci_queue;
	u_int cpu = CPU_INFO_UNIT(ci);

	db_show_clockintr(cq->cq_next_hardclock, cpu, "hardclock");
	db_show_clockintr(cq->cq_next_statclock, cpu, "statclock");
	if (ISSET(clockintr_flags, CL_SCHEDCLOCK))
		db_show_clockintr(cq->cq_next_schedclock, cpu, "schedclock");
}

void
db_show_clockintr(uint64_t expiration, u_int cpu, const char *name)
{
	struct timespec ts;

	NSEC_TO_TIMESPEC(expiration, &ts);
	db_printf("%10lld.%09ld  %3u  %s\n", ts.tv_sec, ts.tv_nsec, cpu, name);
}

#endif /* DDB */
#endif /*__HAVE_CLOCKINTR */

/*	$OpenBSD: kern_tc.c,v 1.55 2019/12/12 19:30:21 cheloha Exp $ */

/*
 * Copyright (c) 2000 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * If we meet some day, and you think this stuff is worth it, you
 * can buy me a beer in return. Poul-Henning Kamp
 */

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/stdint.h>
#include <sys/timeout.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <dev/rndvar.h>

/*
 * A large step happens on boot.  This constant detects such steps.
 * It is relatively small so that ntp_update_second gets called enough
 * in the typical 'missed a couple of seconds' case, but doesn't loop
 * forever when the time step is large.
 */
#define LARGE_STEP	200

u_int dummy_get_timecount(struct timecounter *);

int sysctl_tc_hardware(void *, size_t *, void *, size_t);
int sysctl_tc_choice(void *, size_t *, void *, size_t);

/*
 * Implement a dummy timecounter which we can use until we get a real one
 * in the air.  This allows the console and other early stuff to use
 * time services.
 */

u_int
dummy_get_timecount(struct timecounter *tc)
{
	static u_int now;

	return (++now);
}

static struct timecounter dummy_timecounter = {
	dummy_get_timecount, 0, ~0u, 1000000, "dummy", -1000000
};

/*
 * Locks used to protect struct members, global variables in this file:
 *	I	immutable after initialization
 *	t	tc_lock
 *	w	windup_mtx
 */

struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter;		/* [w] */
	int64_t			th_adjtimedelta;	/* [tw] */
	int64_t			th_adjustment;		/* [w] */
	u_int64_t		th_scale;		/* [w] */
	u_int	 		th_offset_count;	/* [w] */
	struct bintime		th_boottime;		/* [tw] */
	struct bintime		th_offset;		/* [w] */
	struct timeval		th_microtime;		/* [w] */
	struct timespec		th_nanotime;		/* [w] */
	/* Fields not to be copied in tc_windup start with th_generation. */
	volatile u_int		th_generation;		/* [w] */
	struct timehands	*th_next;		/* [I] */
};

static struct timehands th0;
static struct timehands th1 = {
	.th_next = &th0
};
static struct timehands th0 = {
	.th_counter = &dummy_timecounter,
	.th_scale = UINT64_MAX / 1000000,
	.th_offset = { .sec = 1, .frac = 0 },
	.th_generation = 1,
	.th_next = &th1
};

struct rwlock tc_lock = RWLOCK_INITIALIZER("tc_lock");

/*
 * tc_windup() must be called before leaving this mutex.
 */
struct mutex windup_mtx = MUTEX_INITIALIZER(IPL_CLOCK);

static struct timehands *volatile timehands = &th0;		/* [w] */
struct timecounter *timecounter = &dummy_timecounter;		/* [t] */
static SLIST_HEAD(, timecounter) tc_list = SLIST_HEAD_INITIALIZER(tc_list);

volatile time_t time_second = 1;
volatile time_t time_uptime = 0;

struct bintime naptime;
static int timestepwarnings;

void ntp_update_second(struct timehands *);
void tc_windup(struct bintime *, struct bintime *, int64_t *);

/*
 * Return the difference between the timehands' counter value now and what
 * was when we copied it to the timehands' offset_count.
 */
static __inline u_int
tc_delta(struct timehands *th)
{
	struct timecounter *tc;

	tc = th->th_counter;
	return ((tc->tc_get_timecount(tc) - th->th_offset_count) &
	    tc->tc_counter_mask);
}

/*
 * Functions for reading the time.  We have to loop until we are sure that
 * the timehands that we operated on was not updated under our feet.  See
 * the comment in <sys/time.h> for a description of these functions.
 */

void
binboottime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		*bt = th->th_boottime;
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
microboottime(struct timeval *tvp)
{
	struct bintime bt;
	
	binboottime(&bt);
	BINTIME_TO_TIMEVAL(&bt, tvp);
}

void
nanoboottime(struct timespec *tsp)
{
	struct bintime bt;
	
	binboottime(&bt);
	BINTIME_TO_TIMESPEC(&bt, tsp);
}

void
binuptime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		*bt = th->th_offset;
		bintimeaddfrac(bt, th->th_scale * tc_delta(th), bt);
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
nanouptime(struct timespec *tsp)
{
	struct bintime bt;

	binuptime(&bt);
	BINTIME_TO_TIMESPEC(&bt, tsp);
}

void
microuptime(struct timeval *tvp)
{
	struct bintime bt;

	binuptime(&bt);
	BINTIME_TO_TIMEVAL(&bt, tvp);
}

void
bintime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		*bt = th->th_offset;
		bintimeaddfrac(bt, th->th_scale * tc_delta(th), bt);
		bintimeadd(bt, &th->th_boottime, bt);
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
nanotime(struct timespec *tsp)
{
	struct bintime bt;

	bintime(&bt);
	BINTIME_TO_TIMESPEC(&bt, tsp);
}

void
microtime(struct timeval *tvp)
{
	struct bintime bt;

	bintime(&bt);
	BINTIME_TO_TIMEVAL(&bt, tvp);
}

void
getnanouptime(struct timespec *tsp)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		BINTIME_TO_TIMESPEC(&th->th_offset, tsp);
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
getmicrouptime(struct timeval *tvp)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		BINTIME_TO_TIMEVAL(&th->th_offset, tvp);
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
getnanotime(struct timespec *tsp)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		*tsp = th->th_nanotime;
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

void
getmicrotime(struct timeval *tvp)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		membar_consumer();
		*tvp = th->th_microtime;
		membar_consumer();
	} while (gen == 0 || gen != th->th_generation);
}

/*
 * Initialize a new timecounter and possibly use it.
 */
void
tc_init(struct timecounter *tc)
{
	u_int64_t tmp;
	u_int u;

	u = tc->tc_frequency / tc->tc_counter_mask;
	/* XXX: We need some margin here, 10% is a guess */
	u *= 11;
	u /= 10;
	if (tc->tc_quality >= 0) {
		if (u > hz) {
			tc->tc_quality = -2000;
			printf("Timecounter \"%s\" frequency %lu Hz",
			    tc->tc_name, (unsigned long)tc->tc_frequency);
			printf(" -- Insufficient hz, needs at least %u\n", u);
		}
	}

	/* Determine the counter's precision. */
	for (tmp = 1; (tmp & tc->tc_counter_mask) == 0; tmp <<= 1)
		continue;
	tc->tc_precision = tmp;

	SLIST_INSERT_HEAD(&tc_list, tc, tc_next);

	/*
	 * Never automatically use a timecounter with negative quality.
	 * Even though we run on the dummy counter, switching here may be
	 * worse since this timecounter may not be monotonic.
	 */
	if (tc->tc_quality < 0)
		return;
	if (tc->tc_quality < timecounter->tc_quality)
		return;
	if (tc->tc_quality == timecounter->tc_quality &&
	    tc->tc_frequency < timecounter->tc_frequency)
		return;
	(void)tc->tc_get_timecount(tc);
	enqueue_randomness(tc->tc_get_timecount(tc));

	timecounter = tc;
}

/* Report the frequency of the current timecounter. */
u_int64_t
tc_getfrequency(void)
{
	return (timehands->th_counter->tc_frequency);
}

/* Report the precision of the current timecounter. */
u_int64_t
tc_getprecision(void)
{
	return (timehands->th_counter->tc_precision);
}

/*
 * Step our concept of UTC, aka the realtime clock.
 * This is done by modifying our estimate of when we booted.
 *
 * Any ongoing adjustment is meaningless after a clock jump,
 * so we zero adjtimedelta here as well.
 */
void
tc_setrealtimeclock(const struct timespec *ts)
{
	struct timespec ts2;
	struct bintime bt, bt2;
	int64_t zero = 0;

	rw_enter_write(&tc_lock);
	mtx_enter(&windup_mtx);
	binuptime(&bt2);
	TIMESPEC_TO_BINTIME(ts, &bt);
	bintimesub(&bt, &bt2, &bt);
	bintimeadd(&bt2, &timehands->th_boottime, &bt2);

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup(&bt, NULL, &zero);
	mtx_leave(&windup_mtx);
	rw_exit_write(&tc_lock);

	enqueue_randomness(ts->tv_sec);

	if (timestepwarnings) {
		BINTIME_TO_TIMESPEC(&bt2, &ts2);
		log(LOG_INFO, "Time stepped from %lld.%09ld to %lld.%09ld\n",
		    (long long)ts2.tv_sec, ts2.tv_nsec,
		    (long long)ts->tv_sec, ts->tv_nsec);
	}
}

/*
 * Step the monotonic and realtime clocks, triggering any timeouts that
 * should have occurred across the interval.
 */
void
tc_setclock(const struct timespec *ts)
{
	struct bintime bt, bt2;
	struct timespec earlier;
	static int first = 1;
	int rewind = 0;
#ifndef SMALL_KERNEL
	long long adj_ticks;
#endif

	/*
	 * When we're called for the first time, during boot when
	 * the root partition is mounted, we need to set boottime.
	 */
	if (first) {
		tc_setrealtimeclock(ts);
		first = 0;
		return;
	}

	enqueue_randomness(ts->tv_sec);

	mtx_enter(&windup_mtx);
	TIMESPEC_TO_BINTIME(ts, &bt);
	bintimesub(&bt, &timehands->th_boottime, &bt);

	/*
	 * Don't rewind the offset.
	 */
	if (bintimecmp(&bt, &timehands->th_offset, <))
		rewind = 1;

	bt2 = timehands->th_offset;

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup(NULL, rewind ? NULL : &bt, NULL);
	mtx_leave(&windup_mtx);

	if (rewind) {
		BINTIME_TO_TIMESPEC(&bt, &earlier);
		printf("%s: cannot rewind uptime to %lld.%09ld\n",
		    __func__, (long long)earlier.tv_sec, earlier.tv_nsec);
		return;
	}

#ifndef SMALL_KERNEL
	/* convert the bintime to ticks */
	bintimesub(&bt, &bt2, &bt);
	bintimeadd(&naptime, &bt, &naptime);
	adj_ticks = (uint64_t)hz * bt.sec +
	    (((uint64_t)1000000 * (uint32_t)(bt.frac >> 32)) >> 32) / tick;
	if (adj_ticks > 0) {
		if (adj_ticks > INT_MAX)
			adj_ticks = INT_MAX;
		timeout_adjust_ticks(adj_ticks);
	}
#endif
}

/*
 * Initialize the next struct timehands in the ring and make
 * it the active timehands.  Along the way we might switch to a different
 * timecounter and/or do seconds processing in NTP.  Slightly magic.
 */
void
tc_windup(struct bintime *new_boottime, struct bintime *new_offset,
    int64_t *new_adjtimedelta)
{
	struct bintime bt;
	struct timecounter *active_tc;
	struct timehands *th, *tho;
	u_int64_t scale;
	u_int delta, ncount, ogen;
	int i;

	if (new_boottime != NULL || new_adjtimedelta != NULL)
		rw_assert_wrlock(&tc_lock);
	MUTEX_ASSERT_LOCKED(&windup_mtx);

	active_tc = timecounter;

	/*
	 * Make the next timehands a copy of the current one, but do not
	 * overwrite the generation or next pointer.  While we update
	 * the contents, the generation must be zero.
	 */
	tho = timehands;
	th = tho->th_next;
	ogen = th->th_generation;
	th->th_generation = 0;
	membar_producer();
	memcpy(th, tho, offsetof(struct timehands, th_generation));

	/*
	 * If changing the boot offset, do so before updating the
	 * offset fields.
	 */
	if (new_offset != NULL)
		th->th_offset = *new_offset;

	/*
	 * Capture a timecounter delta on the current timecounter and if
	 * changing timecounters, a counter value from the new timecounter.
	 * Update the offset fields accordingly.
	 */
	delta = tc_delta(th);
	if (th->th_counter != active_tc)
		ncount = active_tc->tc_get_timecount(active_tc);
	else
		ncount = 0;
	th->th_offset_count += delta;
	th->th_offset_count &= th->th_counter->tc_counter_mask;
	bintimeaddfrac(&th->th_offset, th->th_scale * delta, &th->th_offset);

#ifdef notyet
	/*
	 * Hardware latching timecounters may not generate interrupts on
	 * PPS events, so instead we poll them.  There is a finite risk that
	 * the hardware might capture a count which is later than the one we
	 * got above, and therefore possibly in the next NTP second which might
	 * have a different rate than the current NTP second.  It doesn't
	 * matter in practice.
	 */
	if (tho->th_counter->tc_poll_pps)
		tho->th_counter->tc_poll_pps(tho->th_counter);
#endif

	/*
	 * If changing the boot time or clock adjustment, do so before
	 * NTP processing.
	 */
	if (new_boottime != NULL)
		th->th_boottime = *new_boottime;
	if (new_adjtimedelta != NULL)
		th->th_adjtimedelta = *new_adjtimedelta;

	/*
	 * Deal with NTP second processing.  The for loop normally
	 * iterates at most once, but in extreme situations it might
	 * keep NTP sane if timeouts are not run for several seconds.
	 * At boot, the time step can be large when the TOD hardware
	 * has been read, so on really large steps, we call
	 * ntp_update_second only twice.  We need to call it twice in
	 * case we missed a leap second.
	 */
	bt = th->th_offset;
	bintimeadd(&bt, &th->th_boottime, &bt);
	i = bt.sec - tho->th_microtime.tv_sec;
	if (i > LARGE_STEP)
		i = 2;
	for (; i > 0; i--)
		ntp_update_second(th);

	/* Update the UTC timestamps used by the get*() functions. */
	/* XXX shouldn't do this here.  Should force non-`get' versions. */
	BINTIME_TO_TIMEVAL(&bt, &th->th_microtime);
	BINTIME_TO_TIMESPEC(&bt, &th->th_nanotime);

	/* Now is a good time to change timecounters. */
	if (th->th_counter != active_tc) {
		th->th_counter = active_tc;
		th->th_offset_count = ncount;
	}

	/*-
	 * Recalculate the scaling factor.  We want the number of 1/2^64
	 * fractions of a second per period of the hardware counter, taking
	 * into account the th_adjustment factor which the NTP PLL/adjtime(2)
	 * processing provides us with.
	 *
	 * The th_adjustment is nanoseconds per second with 32 bit binary
	 * fraction and we want 64 bit binary fraction of second:
	 *
	 *	 x = a * 2^32 / 10^9 = a * 4.294967296
	 *
	 * The range of th_adjustment is +/- 5000PPM so inside a 64bit int
	 * we can only multiply by about 850 without overflowing, but that
	 * leaves suitably precise fractions for multiply before divide.
	 *
	 * Divide before multiply with a fraction of 2199/512 results in a
	 * systematic undercompensation of 10PPM of th_adjustment.  On a
	 * 5000PPM adjustment this is a 0.05PPM error.  This is acceptable.
 	 *
	 * We happily sacrifice the lowest of the 64 bits of our result
	 * to the goddess of code clarity.
	 *
	 */
	scale = (u_int64_t)1 << 63;
	scale += \
	    ((th->th_adjustment + th->th_counter->tc_freq_adj) / 1024) * 2199;
	scale /= th->th_counter->tc_frequency;
	th->th_scale = scale * 2;

	/*
	 * Now that the struct timehands is again consistent, set the new
	 * generation number, making sure to not make it zero.
	 */
	if (++ogen == 0)
		ogen = 1;
	membar_producer();
	th->th_generation = ogen;

	/* Go live with the new struct timehands. */
	time_second = th->th_microtime.tv_sec;
	time_uptime = th->th_offset.sec;
	membar_producer();
	timehands = th;
}

/* Report or change the active timecounter hardware. */
int
sysctl_tc_hardware(void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	char newname[32];
	struct timecounter *newtc, *tc;
	int error;

	tc = timecounter;
	strlcpy(newname, tc->tc_name, sizeof(newname));

	error = sysctl_string(oldp, oldlenp, newp, newlen, newname, sizeof(newname));
	if (error != 0 || strcmp(newname, tc->tc_name) == 0)
		return (error);
	SLIST_FOREACH(newtc, &tc_list, tc_next) {
		if (strcmp(newname, newtc->tc_name) != 0)
			continue;

		/* Warm up new timecounter. */
		(void)newtc->tc_get_timecount(newtc);
		(void)newtc->tc_get_timecount(newtc);

		rw_enter_write(&tc_lock);
		timecounter = newtc;
		rw_exit_write(&tc_lock);

		return (0);
	}
	return (EINVAL);
}

/* Report or change the active timecounter hardware. */
int
sysctl_tc_choice(void *oldp, size_t *oldlenp, void *newp, size_t newlen)
{
	char buf[32], *spc, *choices;
	struct timecounter *tc;
	int error, maxlen;

	if (SLIST_EMPTY(&tc_list))
		return (sysctl_rdstring(oldp, oldlenp, newp, ""));

	spc = "";
	maxlen = 0;
	SLIST_FOREACH(tc, &tc_list, tc_next)
		maxlen += sizeof(buf);
	choices = malloc(maxlen, M_TEMP, M_WAITOK);
	*choices = '\0';
	SLIST_FOREACH(tc, &tc_list, tc_next) {
		snprintf(buf, sizeof(buf), "%s%s(%d)",
		    spc, tc->tc_name, tc->tc_quality);
		spc = " ";
		strlcat(choices, buf, maxlen);
	}
	error = sysctl_rdstring(oldp, oldlenp, newp, choices);
	free(choices, M_TEMP, maxlen);
	return (error);
}

/*
 * Timecounters need to be updated every so often to prevent the hardware
 * counter from overflowing.  Updating also recalculates the cached values
 * used by the get*() family of functions, so their precision depends on
 * the update frequency.
 */
static int tc_tick;

void
tc_ticktock(void)
{
	static int count;

	if (++count < tc_tick)
		return;
	if (!mtx_enter_try(&windup_mtx))
		return;
	count = 0;
	tc_windup(NULL, NULL, NULL);
	mtx_leave(&windup_mtx);
}

void
inittimecounter(void)
{
#ifdef DEBUG
	u_int p;
#endif

	/*
	 * Set the initial timeout to
	 * max(1, <approx. number of hardclock ticks in a millisecond>).
	 * People should probably not use the sysctl to set the timeout
	 * to smaller than its initial value, since that value is the
	 * smallest reasonable one.  If they want better timestamps they
	 * should use the non-"get"* functions.
	 */
	if (hz > 1000)
		tc_tick = (hz + 500) / 1000;
	else
		tc_tick = 1;
#ifdef DEBUG
	p = (tc_tick * 1000000) / hz;
	printf("Timecounters tick every %d.%03u msec\n", p / 1000, p % 1000);
#endif

	/* warm up new timecounter (again) and get rolling. */
	(void)timecounter->tc_get_timecount(timecounter);
	(void)timecounter->tc_get_timecount(timecounter);
}

/*
 * Return timecounter-related information.
 */
int
sysctl_tc(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case KERN_TIMECOUNTER_TICK:
		return (sysctl_rdint(oldp, oldlenp, newp, tc_tick));
	case KERN_TIMECOUNTER_TIMESTEPWARNINGS:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &timestepwarnings));
	case KERN_TIMECOUNTER_HARDWARE:
		return (sysctl_tc_hardware(oldp, oldlenp, newp, newlen));
	case KERN_TIMECOUNTER_CHOICE:
		return (sysctl_tc_choice(oldp, oldlenp, newp, newlen));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Skew the timehands according to any adjtime(2) adjustment.
 */
void
ntp_update_second(struct timehands *th)
{
	int64_t adj;

	MUTEX_ASSERT_LOCKED(&windup_mtx);

	if (th->th_adjtimedelta > 0)
		adj = MIN(5000, th->th_adjtimedelta);
	else
		adj = MAX(-5000, th->th_adjtimedelta);
	th->th_adjtimedelta -= adj;
	th->th_adjustment = (adj * 1000) << 32;
}

void
tc_adjfreq(int64_t *old, int64_t *new)
{
	if (old != NULL) {
		rw_assert_anylock(&tc_lock);
		*old = timecounter->tc_freq_adj;
	}
	if (new != NULL) {
		rw_assert_wrlock(&tc_lock);
		mtx_enter(&windup_mtx);
		timecounter->tc_freq_adj = *new;
		tc_windup(NULL, NULL, NULL);
		mtx_leave(&windup_mtx);
	}
}

void
tc_adjtime(int64_t *old, int64_t *new)
{
	struct timehands *th;
	u_int gen;

	if (old != NULL) {
		do {
			th = timehands;
			gen = th->th_generation;
			membar_consumer();
			*old = th->th_adjtimedelta;
			membar_consumer();
		} while (gen == 0 || gen != th->th_generation);
	}
	if (new != NULL) {
		rw_assert_wrlock(&tc_lock);
		mtx_enter(&windup_mtx);
		tc_windup(NULL, NULL, new);
		mtx_leave(&windup_mtx);
	}
}

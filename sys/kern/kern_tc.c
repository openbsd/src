/*-
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $OpenBSD: kern_tc.c,v 1.14 2010/04/20 22:05:43 tedu Exp $
 * $FreeBSD: src/sys/kern/kern_tc.c,v 1.148 2003/03/18 08:45:23 phk Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/timetc.h>
#include <sys/malloc.h>
#include <dev/rndvar.h>

#ifdef __HAVE_TIMECOUNTER
/*
 * A large step happens on boot.  This constant detects such steps.
 * It is relatively small so that ntp_update_second gets called enough
 * in the typical 'missed a couple of seconds' case, but doesn't loop
 * forever when the time step is large.
 */
#define LARGE_STEP	200

u_int dummy_get_timecount(struct timecounter *);

void ntp_update_second(int64_t *, time_t *);
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

struct timehands {
	/* These fields must be initialized by the driver. */
	struct timecounter	*th_counter;
	int64_t			th_adjustment;
	u_int64_t		th_scale;
	u_int	 		th_offset_count;
	struct bintime		th_offset;
	struct timeval		th_microtime;
	struct timespec		th_nanotime;
	/* Fields not to be copied in tc_windup start with th_generation. */
	volatile u_int		th_generation;
	struct timehands	*th_next;
};

static struct timehands th0;
static struct timehands th9 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th0};
static struct timehands th8 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th9};
static struct timehands th7 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th8};
static struct timehands th6 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th7};
static struct timehands th5 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th6};
static struct timehands th4 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th5};
static struct timehands th3 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th4};
static struct timehands th2 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th3};
static struct timehands th1 = { NULL, 0, 0, 0, {0, 0}, {0, 0}, {0, 0}, 0, &th2};
static struct timehands th0 = {
	&dummy_timecounter,
	0,
	(uint64_t)-1 / 1000000,
	0,
	{1, 0},
	{0, 0},
	{0, 0},
	1,
	&th1
};

static struct timehands *volatile timehands = &th0;
struct timecounter *timecounter = &dummy_timecounter;
static struct timecounter *timecounters = &dummy_timecounter;

volatile time_t time_second = 1;
volatile time_t time_uptime = 0;

extern struct timeval adjtimedelta;
static struct bintime boottimebin;
static int timestepwarnings;

void tc_windup(void);

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
 * the comment in <sys/time.h> for a description of these 12 functions.
 */

void
binuptime(struct bintime *bt)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		*bt = th->th_offset;
		bintime_addx(bt, th->th_scale * tc_delta(th));
	} while (gen == 0 || gen != th->th_generation);
}

void
nanouptime(struct timespec *tsp)
{
	struct bintime bt;

	binuptime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microuptime(struct timeval *tvp)
{
	struct bintime bt;

	binuptime(&bt);
	bintime2timeval(&bt, tvp);
}

void
bintime(struct bintime *bt)
{

	binuptime(bt);
	bintime_add(bt, &boottimebin);
}

void
nanotime(struct timespec *tsp)
{
	struct bintime bt;

	bintime(&bt);
	bintime2timespec(&bt, tsp);
}

void
microtime(struct timeval *tvp)
{
	struct bintime bt;

	bintime(&bt);
	bintime2timeval(&bt, tvp);
}

void
getnanouptime(struct timespec *tsp)
{
	struct timehands *th;
	u_int gen;

	do {
		th = timehands;
		gen = th->th_generation;
		bintime2timespec(&th->th_offset, tsp);
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
		bintime2timeval(&th->th_offset, tvp);
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
		*tsp = th->th_nanotime;
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
		*tvp = th->th_microtime;
	} while (gen == 0 || gen != th->th_generation);
}

/*
 * Initialize a new timecounter and possibly use it.
 */
void
tc_init(struct timecounter *tc)
{
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

	tc->tc_next = timecounters;
	timecounters = tc;
	/*
	 * Never automatically use a timecounter with negative quality.
	 * Even though we run on the dummy counter, switching here may be
	 * worse since this timecounter may not be monotonous.
	 */
	if (tc->tc_quality < 0)
		return;
	if (tc->tc_quality < timecounter->tc_quality)
		return;
	if (tc->tc_quality == timecounter->tc_quality &&
	    tc->tc_frequency < timecounter->tc_frequency)
		return;
	(void)tc->tc_get_timecount(tc);
	add_timer_randomness(tc->tc_get_timecount(tc));

	timecounter = tc;
}

/* Report the frequency of the current timecounter. */
u_int64_t
tc_getfrequency(void)
{

	return (timehands->th_counter->tc_frequency);
}

/*
 * Step our concept of UTC.  This is done by modifying our estimate of
 * when we booted.
 * XXX: not locked.
 */
void
tc_setclock(struct timespec *ts)
{
	struct timespec ts2;
	struct bintime bt, bt2;

	binuptime(&bt2);
	timespec2bintime(ts, &bt);
	bintime_sub(&bt, &bt2);
	bintime_add(&bt2, &boottimebin);
	boottimebin = bt;
	bintime2timeval(&bt, &boottime);
	add_timer_randomness(ts->tv_sec);

	/* XXX fiddle all the little crinkly bits around the fiords... */
	tc_windup();
	if (timestepwarnings) {
		bintime2timespec(&bt2, &ts2);
		log(LOG_INFO, "Time stepped from %ld.%09ld to %ld.%09ld\n",
		    (long)ts2.tv_sec, ts2.tv_nsec,
		    (long)ts->tv_sec, ts->tv_nsec);
	}
}

/*
 * Initialize the next struct timehands in the ring and make
 * it the active timehands.  Along the way we might switch to a different
 * timecounter and/or do seconds processing in NTP.  Slightly magic.
 */
void
tc_windup(void)
{
	struct bintime bt;
	struct timehands *th, *tho;
	u_int64_t scale;
	u_int delta, ncount, ogen;
	int i;
#ifdef leapsecs
	time_t t;
#endif

	/*
	 * Make the next timehands a copy of the current one, but do not
	 * overwrite the generation or next pointer.  While we update
	 * the contents, the generation must be zero.
	 */
	tho = timehands;
	th = tho->th_next;
	ogen = th->th_generation;
	th->th_generation = 0;
	bcopy(tho, th, offsetof(struct timehands, th_generation));

	/*
	 * Capture a timecounter delta on the current timecounter and if
	 * changing timecounters, a counter value from the new timecounter.
	 * Update the offset fields accordingly.
	 */
	delta = tc_delta(th);
	if (th->th_counter != timecounter)
		ncount = timecounter->tc_get_timecount(timecounter);
	else
		ncount = 0;
	th->th_offset_count += delta;
	th->th_offset_count &= th->th_counter->tc_counter_mask;
	bintime_addx(&th->th_offset, th->th_scale * delta);

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
	 * Deal with NTP second processing.  The for loop normally
	 * iterates at most once, but in extreme situations it might
	 * keep NTP sane if timeouts are not run for several seconds.
	 * At boot, the time step can be large when the TOD hardware
	 * has been read, so on really large steps, we call
	 * ntp_update_second only twice.  We need to call it twice in
	 * case we missed a leap second.
	 */
	bt = th->th_offset;
	bintime_add(&bt, &boottimebin);
	i = bt.sec - tho->th_microtime.tv_sec;
	if (i > LARGE_STEP)
		i = 2;
	for (; i > 0; i--)
		ntp_update_second(&th->th_adjustment, &bt.sec);

	/* Update the UTC timestamps used by the get*() functions. */
	/* XXX shouldn't do this here.  Should force non-`get' versions. */
	bintime2timeval(&bt, &th->th_microtime);
	bintime2timespec(&bt, &th->th_nanotime);

	/* Now is a good time to change timecounters. */
	if (th->th_counter != timecounter) {
		th->th_counter = timecounter;
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
	scale += (th->th_adjustment / 1024) * 2199;
	scale /= th->th_counter->tc_frequency;
	th->th_scale = scale * 2;

	/*
	 * Now that the struct timehands is again consistent, set the new
	 * generation number, making sure to not make it zero.
	 */
	if (++ogen == 0)
		ogen = 1;
	th->th_generation = ogen;

	/* Go live with the new struct timehands. */
	time_second = th->th_microtime.tv_sec;
	time_uptime = th->th_offset.sec;
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
	for (newtc = timecounters; newtc != NULL; newtc = newtc->tc_next) {
		if (strcmp(newname, newtc->tc_name) != 0)
			continue;

		/* Warm up new timecounter. */
		(void)newtc->tc_get_timecount(newtc);
		(void)newtc->tc_get_timecount(newtc);

		timecounter = newtc;
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

	spc = "";
	error = 0;
	maxlen = 0;
	for (tc = timecounters; tc != NULL; tc = tc->tc_next)
		maxlen += sizeof(buf);
	choices = malloc(maxlen, M_TEMP, M_WAITOK);
	*choices = '\0';
	for (tc = timecounters; tc != NULL; tc = tc->tc_next) {
		snprintf(buf, sizeof(buf), "%s%s(%d)",
		    spc, tc->tc_name, tc->tc_quality);
		spc = " ";
		strlcat(choices, buf, maxlen);
	}
	error = sysctl_rdstring(oldp, oldlenp, newp, choices);
	free(choices, M_TEMP);
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
	count = 0;
	tc_windup();
}

void
inittimecounter(void)
{
	u_int p;

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
	p = (tc_tick * 1000000) / hz;
#ifdef DEBUG
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

void
ntp_update_second(int64_t *adjust, time_t *sec)
{
	struct timeval adj;

	/* Skew time according to any adjtime(2) adjustments. */
	timerclear(&adj);
	if (adjtimedelta.tv_sec > 0)
		adj.tv_usec = 5000;
	else if (adjtimedelta.tv_sec == 0)
		adj.tv_usec = MIN(5000, adjtimedelta.tv_usec);
	else if (adjtimedelta.tv_sec < -1)
		adj.tv_usec = -5000;
	else if (adjtimedelta.tv_sec == -1)
		adj.tv_usec = MAX(-5000, adjtimedelta.tv_usec - 1000000);
	timersub(&adjtimedelta, &adj, &adjtimedelta);
	*adjust = ((int64_t)adj.tv_usec * 1000) << 32;
	*adjust += timecounter->tc_freq_adj;
}

int
tc_adjfreq(int64_t *old, int64_t *new)
{
	if (old != NULL) {
		*old = timecounter->tc_freq_adj;
	}
	if (new != NULL) {
		timecounter->tc_freq_adj = *new;
	}
	return 0;
}
#endif /* __HAVE_TIMECOUNTER */

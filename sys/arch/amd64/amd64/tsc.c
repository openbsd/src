/*	$OpenBSD: tsc.c,v 1.25 2022/08/12 02:20:36 cheloha Exp $	*/
/*
 * Copyright (c) 2008 The NetBSD Foundation, Inc.
 * Copyright (c) 2016,2017 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2017 Adam Steen <adam@adamsteen.com.au>
 * Copyright (c) 2017 Mike Belopuhov <mike@openbsd.org>
 * Copyright (c) 2019 Paul Irofti <paul@irofti.net>
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
#include <sys/timetc.h>
#include <sys/atomic.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>

#define RECALIBRATE_MAX_RETRIES		5
#define RECALIBRATE_SMI_THRESHOLD	50000
#define RECALIBRATE_DELAY_THRESHOLD	50

int		tsc_recalibrate;

uint64_t	tsc_frequency;
int		tsc_is_invariant;

u_int		tsc_get_timecount(struct timecounter *tc);
void		tsc_delay(int usecs);

#include "lapic.h"
#if NLAPIC > 0
extern u_int32_t lapic_per_second;
#endif

struct timecounter tsc_timecounter = {
	.tc_get_timecount = tsc_get_timecount,
	.tc_poll_pps = NULL,
	.tc_counter_mask = ~0u,
	.tc_frequency = 0,
	.tc_name = "tsc",
	.tc_quality = -1000,
	.tc_priv = NULL,
	.tc_user = TC_TSC,
};

uint64_t
tsc_freq_cpuid(struct cpu_info *ci)
{
	uint64_t count;
	uint32_t eax, ebx, khz, dummy;

	if (!strcmp(cpu_vendor, "GenuineIntel") &&
	    cpuid_level >= 0x15) {
		eax = ebx = khz = dummy = 0;
		CPUID(0x15, eax, ebx, khz, dummy);
		khz /= 1000;
		if (khz == 0) {
			switch (ci->ci_model) {
			case 0x4e: /* Skylake mobile */
			case 0x5e: /* Skylake desktop */
			case 0x8e: /* Kabylake mobile */
			case 0x9e: /* Kabylake desktop */
			case 0xa5: /* CML-H CML-S62 CML-S102 */
			case 0xa6: /* CML-U62 */
				khz = 24000; /* 24.0 MHz */
				break;
			case 0x5f: /* Atom Denverton */
				khz = 25000; /* 25.0 MHz */
				break;
			case 0x5c: /* Atom Goldmont */
				khz = 19200; /* 19.2 MHz */
				break;
			}
		}
		if (ebx == 0 || eax == 0)
			count = 0;
		else if ((count = (uint64_t)khz * (uint64_t)ebx / eax) != 0) {
#if NLAPIC > 0
			lapic_per_second = khz * 1000;
#endif
			return (count * 1000);
		}
	}

	return (0);
}

void
tsc_identify(struct cpu_info *ci)
{
	if (!(ci->ci_flags & CPUF_PRIMARY) ||
	    !(ci->ci_flags & CPUF_CONST_TSC) ||
	    !(ci->ci_flags & CPUF_INVAR_TSC))
		return;

	tsc_is_invariant = 1;

	tsc_frequency = tsc_freq_cpuid(ci);
	if (tsc_frequency > 0)
		delay_func = tsc_delay;
}

static inline int
get_tsc_and_timecount(struct timecounter *tc, uint64_t *tsc, uint64_t *count)
{
	uint64_t n, tsc1, tsc2;
	int i;

	for (i = 0; i < RECALIBRATE_MAX_RETRIES; i++) {
		tsc1 = rdtsc_lfence();
		n = (tc->tc_get_timecount(tc) & tc->tc_counter_mask);
		tsc2 = rdtsc_lfence();

		if ((tsc2 - tsc1) < RECALIBRATE_SMI_THRESHOLD) {
			*count = n;
			*tsc = tsc2;
			return (0);
		}
	}
	return (1);
}

static inline uint64_t
calculate_tsc_freq(uint64_t tsc1, uint64_t tsc2, int usec)
{
	uint64_t delta;

	delta = (tsc2 - tsc1);
	return (delta * 1000000 / usec);
}

static inline uint64_t
calculate_tc_delay(struct timecounter *tc, uint64_t count1, uint64_t count2)
{
	uint64_t delta;

	if (count2 < count1)
		count2 += tc->tc_counter_mask;

	delta = (count2 - count1);
	return (delta * 1000000 / tc->tc_frequency);
}

uint64_t
measure_tsc_freq(struct timecounter *tc)
{
	uint64_t count1, count2, frequency, min_freq, tsc1, tsc2;
	u_long s;
	int delay_usec, i, err1, err2, usec, success = 0;

	/* warmup the timers */
	for (i = 0; i < 3; i++) {
		(void)tc->tc_get_timecount(tc);
		(void)rdtsc();
	}

	min_freq = ULLONG_MAX;

	delay_usec = 100000;
	for (i = 0; i < 3; i++) {
		s = intr_disable();

		err1 = get_tsc_and_timecount(tc, &tsc1, &count1);
		delay(delay_usec);
		err2 = get_tsc_and_timecount(tc, &tsc2, &count2);

		intr_restore(s);

		if (err1 || err2)
			continue;

		usec = calculate_tc_delay(tc, count1, count2);

		if ((usec < (delay_usec - RECALIBRATE_DELAY_THRESHOLD)) ||
		    (usec > (delay_usec + RECALIBRATE_DELAY_THRESHOLD)))
			continue;

		frequency = calculate_tsc_freq(tsc1, tsc2, usec);

		min_freq = MIN(min_freq, frequency);
		success++;
	}

	return (success > 1 ? min_freq : 0);
}

void
calibrate_tsc_freq(void)
{
	struct timecounter *reference = tsc_timecounter.tc_priv;
	uint64_t freq;

	if (!reference || !tsc_recalibrate)
		return;

	if ((freq = measure_tsc_freq(reference)) == 0)
		return;
	tsc_frequency = freq;
	tsc_timecounter.tc_frequency = freq;
	if (tsc_is_invariant)
		tsc_timecounter.tc_quality = 2000;
}

void
cpu_recalibrate_tsc(struct timecounter *tc)
{
	struct timecounter *reference = tsc_timecounter.tc_priv;

	/* Prevent recalibration with a worse timecounter source */
	if (reference && reference->tc_quality > tc->tc_quality)
		return;

	tsc_timecounter.tc_priv = tc;
	calibrate_tsc_freq();
}

u_int
tsc_get_timecount(struct timecounter *tc)
{
	return rdtsc_lfence();
}

void
tsc_timecounter_init(struct cpu_info *ci, uint64_t cpufreq)
{
	if (!(ci->ci_flags & CPUF_PRIMARY) ||
	    !(ci->ci_flags & CPUF_CONST_TSC) ||
	    !(ci->ci_flags & CPUF_INVAR_TSC))
		return;

	/* Newer CPUs don't require recalibration */
	if (tsc_frequency > 0) {
		tsc_timecounter.tc_frequency = tsc_frequency;
		tsc_timecounter.tc_quality = 2000;
	} else {
		tsc_recalibrate = 1;
		tsc_frequency = cpufreq;
		tsc_timecounter.tc_frequency = cpufreq;
		calibrate_tsc_freq();
	}

	tc_init(&tsc_timecounter);
}

void
tsc_delay(int usecs)
{
	uint64_t interval, start;

	interval = (uint64_t)usecs * tsc_frequency / 1000000;
	start = rdtsc_lfence();
	while (rdtsc_lfence() - start < interval)
		CPU_BUSY_CYCLE();
}

#ifdef MULTIPROCESSOR

#define TSC_DEBUG 1

/*
 * Protections for global variables in this code:
 *
 *	a	Modified atomically
 *	b	Protected by a barrier
 *	p	Only modified by the primary CPU
 */

#define TSC_TEST_MSECS		1	/* Test round duration */
#define TSC_TEST_ROUNDS		2	/* Number of test rounds */

/*
 * tsc_test_status.val is isolated to its own cache line to limit
 * false sharing and reduce the test's margin of error.
 */
struct tsc_test_status {
	volatile uint64_t val;		/* [a] Latest RDTSC value */
	uint64_t pad1[7];
	uint64_t lag_count;		/* [b] Number of lags seen by CPU */
	uint64_t lag_max;		/* [b] Biggest lag seen by CPU */
	int64_t adj;			/* [b] Initial IA32_TSC_ADJUST value */
	uint64_t pad2[5];
} __aligned(64);
struct tsc_test_status tsc_ap_status;	/* Test results from AP */
struct tsc_test_status tsc_bp_status;	/* Test results from BP */
uint64_t tsc_test_cycles;		/* [p] TSC cycles per test round */
const char *tsc_ap_name;		/* [b] Name of AP running test */
volatile u_int tsc_egress_barrier;	/* [a] Test end barrier */
volatile u_int tsc_ingress_barrier;	/* [a] Test start barrier */
volatile u_int tsc_test_rounds;		/* [p] Remaining test rounds */
int tsc_is_synchronized = 1;		/* [p] Have we ever failed the test? */

void tsc_report_test_results(void);
void tsc_reset_adjust(struct tsc_test_status *);
void tsc_test_ap(void);
void tsc_test_bp(void);

void
tsc_test_sync_bp(struct cpu_info *ci)
{
	if (!tsc_is_invariant)
		return;
#ifndef TSC_DEBUG
	/* No point in testing again if we already failed. */
	if (!tsc_is_synchronized)
		return;
#endif
	/* Reset IA32_TSC_ADJUST if it exists. */
	tsc_reset_adjust(&tsc_bp_status);

	/* Reset the test cycle limit and round count. */
	tsc_test_cycles = TSC_TEST_MSECS * tsc_frequency / 1000;
	tsc_test_rounds = TSC_TEST_ROUNDS;

	do {
		/*
		 * Pass through the ingress barrier, run the test,
		 * then wait for the AP to reach the egress barrier.
		 */
		atomic_inc_int(&tsc_ingress_barrier);
		while (tsc_ingress_barrier != 2)
			CPU_BUSY_CYCLE();
		tsc_test_bp();
		while (tsc_egress_barrier != 1)
			CPU_BUSY_CYCLE();

		/*
		 * Report what happened.  Adjust the TSC's quality
		 * if this is the first time we've failed the test.
		 */
		tsc_report_test_results();
		if (tsc_ap_status.lag_count || tsc_bp_status.lag_count) {
			if (tsc_is_synchronized) {
				tsc_is_synchronized = 0;
				tc_reset_quality(&tsc_timecounter, -1000);
			}
			tsc_test_rounds = 0;
		} else
			tsc_test_rounds--;

		/*
		 * Clean up for the next round.  It is safe to reset the
		 * ingress barrier because at this point we know the AP
		 * has reached the egress barrier.
		 */
		memset(&tsc_ap_status, 0, sizeof tsc_ap_status);
		memset(&tsc_bp_status, 0, sizeof tsc_bp_status);
		tsc_ingress_barrier = 0;
		if (tsc_test_rounds == 0)
			tsc_ap_name = NULL;

		/*
		 * Pass through the egress barrier and release the AP.
		 * The AP is responsible for resetting the egress barrier.
		 */
		if (atomic_inc_int_nv(&tsc_egress_barrier) != 2)
			panic("%s: unexpected egress count", __func__);
	} while (tsc_test_rounds > 0);
}

void
tsc_test_sync_ap(struct cpu_info *ci)
{
	if (!tsc_is_invariant)
		return;
#ifndef TSC_DEBUG
	if (!tsc_is_synchronized)
		return;
#endif
	/* The BP needs our name in order to report any problems. */
	if (atomic_cas_ptr(&tsc_ap_name, NULL, ci->ci_dev->dv_xname) != NULL) {
		panic("%s: %s: tsc_ap_name is not NULL: %s",
		    __func__, ci->ci_dev->dv_xname, tsc_ap_name);
	}

	tsc_reset_adjust(&tsc_ap_status);

	/*
	 * The AP is only responsible for running the test and
	 * resetting the egress barrier.  The BP handles everything
	 * else.
	 */
	do {
		atomic_inc_int(&tsc_ingress_barrier);
		while (tsc_ingress_barrier != 2)
			CPU_BUSY_CYCLE();
		tsc_test_ap();
		atomic_inc_int(&tsc_egress_barrier);
		while (atomic_cas_uint(&tsc_egress_barrier, 2, 0) != 2)
			CPU_BUSY_CYCLE();
	} while (tsc_test_rounds > 0);
}

void
tsc_report_test_results(void)
{
	u_int round = TSC_TEST_ROUNDS - tsc_test_rounds + 1;

	if (tsc_bp_status.adj != 0) {
		printf("tsc: cpu0: IA32_TSC_ADJUST: %lld -> 0\n",
		    tsc_bp_status.adj);
	}
	if (tsc_ap_status.adj != 0) {
		printf("tsc: %s: IA32_TSC_ADJUST: %lld -> 0\n",
		    tsc_ap_name, tsc_ap_status.adj);
	}
	if (tsc_ap_status.lag_count > 0 || tsc_bp_status.lag_count > 0) {
		printf("tsc: cpu0/%s: sync test round %u/%u failed\n",
		    tsc_ap_name, round, TSC_TEST_ROUNDS);
	}
	if (tsc_bp_status.lag_count > 0) {
		printf("tsc: cpu0/%s: cpu0: %llu lags %llu cycles\n",
		    tsc_ap_name, tsc_bp_status.lag_count,
		    tsc_bp_status.lag_max);
	}
	if (tsc_ap_status.lag_count > 0) {
		printf("tsc: cpu0/%s: %s: %llu lags %llu cycles\n",
		    tsc_ap_name, tsc_ap_name, tsc_ap_status.lag_count,
		    tsc_ap_status.lag_max);
	}
}

/*
 * Reset IA32_TSC_ADJUST if we have it.
 *
 * XXX We should rearrange cpu_hatch() so that the feature
 * flags are already set before we get here.  Check CPUID
 * by hand until then.
 */
void
tsc_reset_adjust(struct tsc_test_status *tts)
{
	uint32_t eax, ebx, ecx, edx;

	CPUID(0, eax, ebx, ecx, edx);
	if (eax >= 7) {
		CPUID_LEAF(7, 0, eax, ebx, ecx, edx);
		if (ISSET(ebx, SEFF0EBX_TSC_ADJUST)) {
			tts->adj = rdmsr(MSR_TSC_ADJUST);
			if (tts->adj != 0)
				wrmsr(MSR_TSC_ADJUST, 0);
		}
	}
}

void
tsc_test_ap(void)
{
	uint64_t ap_val, bp_val, end, lag;

	ap_val = rdtsc_lfence();
	end = ap_val + tsc_test_cycles;
	while (__predict_true(ap_val < end)) {
		/*
		 * Get the BP's latest TSC value, then read the AP's
		 * TSC.  LFENCE is a serializing instruction, so we
		 * know bp_val predates ap_val.  If ap_val is smaller
		 * than bp_val then the AP's TSC must trail that of
		 * the BP and the counters cannot be synchronized.
		 */
		bp_val = tsc_bp_status.val;
		ap_val = rdtsc_lfence();
		tsc_ap_status.val = ap_val;

		/*
		 * Record the magnitude of the problem if the AP's TSC
		 * trails the BP's TSC.
		 */
		if (__predict_false(ap_val < bp_val)) {
			tsc_ap_status.lag_count++;
			lag = bp_val - ap_val;
			if (tsc_ap_status.lag_max < lag)
				tsc_ap_status.lag_max = lag;
		}
	}
}

/*
 * This is similar to tsc_test_ap(), but with all relevant variables
 * flipped around to run the test from the BP's perspective.
 */
void
tsc_test_bp(void)
{
	uint64_t ap_val, bp_val, end, lag;

	bp_val = rdtsc_lfence();
	end = bp_val + tsc_test_cycles;
	while (__predict_true(bp_val < end)) {
		ap_val = tsc_ap_status.val;
		bp_val = rdtsc_lfence();
		tsc_bp_status.val = bp_val;

		if (__predict_false(bp_val < ap_val)) {
			tsc_bp_status.lag_count++;
			lag = ap_val - bp_val;
			if (tsc_bp_status.lag_max < lag)
				tsc_bp_status.lag_max = lag;
		}
	}
}

#endif /* MULTIPROCESSOR */

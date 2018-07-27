/*	$OpenBSD: tsc.c,v 1.10 2018/07/27 21:11:31 kettenis Exp $	*/
/*
 * Copyright (c) 2016,2017 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2017 Adam Steen <adam@adamsteen.com.au>
 * Copyright (c) 2017 Mike Belopuhov <mike@openbsd.org>
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

#include <machine/cpu.h>
#include <machine/cpufunc.h>

#define RECALIBRATE_MAX_RETRIES		5
#define RECALIBRATE_SMI_THRESHOLD	50000
#define RECALIBRATE_DELAY_THRESHOLD	50

int		tsc_recalibrate;

uint64_t	tsc_frequency;
int		tsc_is_invariant;

uint		tsc_get_timecount(struct timecounter *tc);

struct timecounter tsc_timecounter = {
	tsc_get_timecount, NULL, ~0u, 0, "tsc", -1000, NULL
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
		else if ((count = (uint64_t)khz * (uint64_t)ebx / eax) != 0)
			return (count * 1000);
	}

	return (0);
}

static inline int
get_tsc_and_timecount(struct timecounter *tc, uint64_t *tsc, uint64_t *count)
{
	uint64_t n, tsc1, tsc2;
	int i;

	for (i = 0; i < RECALIBRATE_MAX_RETRIES; i++) {
		tsc1 = rdtsc();
		n = (tc->tc_get_timecount(tc) & tc->tc_counter_mask);
		tsc2 = rdtsc();

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

uint
tsc_get_timecount(struct timecounter *tc)
{
	return rdtsc();
}

void
tsc_timecounter_init(struct cpu_info *ci, uint64_t cpufreq)
{
	if (!(ci->ci_flags & CPUF_PRIMARY) ||
	    !(ci->ci_flags & CPUF_CONST_TSC) ||
	    !(ci->ci_flags & CPUF_INVAR_TSC))
		return;

	tsc_frequency = tsc_freq_cpuid(ci);
	tsc_is_invariant = 1;

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

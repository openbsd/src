/*	$OpenBSD: clock.c,v 1.47 2022/10/31 13:59:10 visa Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Clock code for systems using the on-cpu counter register, when both the
 * counter and comparator registers are available (i.e. everything MIPS-III
 * or MIPS-IV capable but the R8000).
 *
 * On most processors, this register counts at half the pipeline frequency.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>

static struct evcount cp0_clock_count;
static int cp0_clock_irq = 5;

int	clockmatch(struct device *, void *, void *);
void	clockattach(struct device *, struct device *, void *);

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

const struct cfattach clock_ca = {
	sizeof(struct device), clockmatch, clockattach
};

void	cp0_startclock(struct cpu_info *);
void	cp0_trigger_int5(void);
uint32_t cp0_int5(uint32_t, struct trapframe *);

int
clockmatch(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	return strcmp(maa->maa_name, clock_cd.cd_name) == 0;
}

void
clockattach(struct device *parent, struct device *self, void *aux)
{
	printf(": int 5\n");

	/*
	 * We need to register the interrupt now, for idle_mask to
	 * be computed correctly.
	 */
	set_intr(INTPRI_CLOCK, CR_INT_5, cp0_int5);
	evcount_attach(&cp0_clock_count, "clock", &cp0_clock_irq);

	/* try to avoid getting clock interrupts early */
	cp0_set_compare(cp0_get_count() - 1);

	md_startclock = cp0_startclock;
	md_triggerclock = cp0_trigger_int5;
}

/*
 *  Interrupt handler for targets using the internal count register
 *  as interval clock. Normally the system is run with the clock
 *  interrupt always enabled. Masking is done here and if the clock
 *  cannot be run the tick is handled later when the clock is logically
 *  unmasked again.
 */
uint32_t
cp0_int5(uint32_t mask, struct trapframe *tf)
{
	u_int32_t clkdiff, pendingticks = 0;
	struct cpu_info *ci = curcpu();
	int s;

	/*
	 * If we got an interrupt before we got ready to process it,
	 * retrigger it as far as possible. cpu_initclocks() will
	 * take care of retriggering it correctly.
	 */
	if (ci->ci_clock_started == 0) {
		cp0_set_compare(cp0_get_count() - 1);

		return CR_INT_5;
	}

	/*
	 * If the clock interrupt is logically masked, defer all
	 * work until it is logically unmasked from splx(9).
	 */
	if (tf->ipl >= IPL_CLOCK) {
		ci->ci_clock_deferred = 1;
		cp0_set_compare(cp0_get_count() - 1);
		return CR_INT_5;
	}
	ci->ci_clock_deferred = 0;

	/*
	 * Count how many ticks have passed since the last clock interrupt...
	 */
	clkdiff = cp0_get_count() - ci->ci_cpu_counter_last;
	while (clkdiff >= ci->ci_cpu_counter_interval) {
		ci->ci_cpu_counter_last += ci->ci_cpu_counter_interval;
		clkdiff = cp0_get_count() - ci->ci_cpu_counter_last;
		pendingticks++;
	}
	pendingticks++;
	ci->ci_cpu_counter_last += ci->ci_cpu_counter_interval;

	/*
	 * Set up next tick, and check if it has just been hit; in this
	 * case count it and schedule one tick ahead.
	 */
	cp0_set_compare(ci->ci_cpu_counter_last);
	clkdiff = cp0_get_count() - ci->ci_cpu_counter_last;
	if ((int)clkdiff >= 0) {
		ci->ci_cpu_counter_last += ci->ci_cpu_counter_interval;
		pendingticks++;
		cp0_set_compare(ci->ci_cpu_counter_last);
	}

	/*
	 * Process clock interrupt.
	 */
	s = splclock();
#ifdef MULTIPROCESSOR
	register_t sr;

	sr = getsr();
	ENABLEIPI();
#endif
	while (pendingticks) {
		atomic_inc_long((unsigned long *)&cp0_clock_count.ec_count);
		hardclock(tf);
		pendingticks--;
	}
#ifdef MULTIPROCESSOR
	setsr(sr);
#endif
	ci->ci_ipl = s;

	return CR_INT_5;	/* Clock is always on 5 */
}

/*
 * Trigger the clock interrupt.
 * 
 * We need to spin until either (a) INT5 is pending or (b) the compare
 * register leads the count register, i.e. we know INT5 will be pending
 * very soon.
 *
 * To ensure we don't spin forever, double the compensatory offset
 * added to the compare value every time we miss the count register.
 * The initial offset of 16 cycles was chosen experimentally.  It
 * is the smallest power of two that doesn't require multiple loops
 * to arm the timer on most Octeon hardware.
 */
void
cp0_trigger_int5(void)
{
	uint32_t compare, offset = 16;
	int leading = 0;
	register_t sr;

	sr = disableintr();
	while (!leading && !ISSET(cp0_get_cause(), CR_INT_5)) {
		compare = cp0_get_count() + offset;
		cp0_set_compare(compare);
		leading = (int32_t)(compare - cp0_get_count()) > 0;
		offset *= 2;
	}
	setsr(sr);
}

/*
 * Start the real-time and statistics clocks. Leave stathz 0 since there
 * are no other timers available.
 */
void
cp0_startclock(struct cpu_info *ci)
{
	int s;

#ifdef MULTIPROCESSOR
	if (!CPU_IS_PRIMARY(ci)) {
		s = splhigh();
		nanouptime(&ci->ci_schedstate.spc_runtime);
		splx(s);

		/* try to avoid getting clock interrupts early */
		cp0_set_compare(cp0_get_count() - 1);

		cp0_calibrate(ci);
	}
#endif

	/* Start the clock. */
	s = splclock();
	ci->ci_cpu_counter_interval =
	    (ci->ci_hw.clock / CP0_CYCLE_DIVIDER) / hz;
	ci->ci_cpu_counter_last = cp0_get_count() + ci->ci_cpu_counter_interval;
	cp0_set_compare(ci->ci_cpu_counter_last);
	ci->ci_clock_started++;
	splx(s);
}

/*	$OpenBSD: clock.c,v 1.17 2012/11/04 13:33:32 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * Copyright (c) 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Lawrence Berkeley Laboratory.
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
 *      @(#)clock.c     8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/timetc.h>

#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include "lrc.h"
#include "mc.h"
#include "ofobio.h"
#include "pcc.h"
#include "pcctwo.h"

#if NLRC > 0
#include <mvme68k/dev/lrcreg.h>
#endif
#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif
#if NOFOBIO > 0
#include <mvme68k/dev/ofobioreg.h>
#endif
#if NPCC > 0
#include <mvme68k/dev/pccreg.h>
#endif
#if NPCCTWO > 0
#include <mvme68k/dev/pcctworeg.h>
#include <mvme68k/dev/vme.h>
extern struct vme2reg *sys_vme2;
#endif

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 8192 would
 * give us offsets in [0..8191].  Instead, we take offsets in [1..8191].
 * This is symmetric about the point 2048, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
int statvar = 8192;
int statmin;		/* statclock interval - 1/2*variance */

struct clocksoftc {
	struct device	sc_dev;
	struct intrhand sc_profih;
	struct intrhand sc_statih;
};

void	clockattach(struct device *, struct device *, void *);
int	clockmatch(struct device *, void *, void *);

struct cfattach clock_ca = {
	sizeof(struct clocksoftc), clockmatch, clockattach
};

struct cfdriver clock_cd = {
	NULL, "clock", DV_DULL
};

int	clockintr(void *);
int	statintr(void *);

int	clockbus;
u_char	stat_reset, prof_reset;

/*
 * Every machine must have a clock tick device of some sort; for this
 * platform this file manages it, no matter what form it takes.
 */
int
clockmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	return (1);
}

void
clockattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct clocksoftc *sc = (struct clocksoftc *)self;

	sc->sc_profih.ih_fn = clockintr;
	sc->sc_profih.ih_arg = 0;
	sc->sc_profih.ih_wantframe = 1;
	sc->sc_profih.ih_ipl = ca->ca_ipl;

	sc->sc_statih.ih_fn = statintr;
	sc->sc_statih.ih_arg = 0;
	sc->sc_statih.ih_wantframe = 1;
	sc->sc_statih.ih_ipl = ca->ca_ipl;

	clockbus = ca->ca_bustype;
	switch (ca->ca_bustype) {
#if NLRC > 0
	case BUS_LRC:
		/*
		 * XXX once we have dynamic ipl levels, put clock at ipl 6,
		 * move it to timer1, then use timer2/ipl5 for statclock.
		 * But then time counters will need to be implemented
		 * differently.
		 */
		lrcintr_establish(LRCVEC_TIMER2, &sc->sc_profih, "clock");
		break;
#endif
#if NMC > 0
	case BUS_MC:
		prof_reset = ca->ca_ipl | MC_IRQ_IEN | MC_IRQ_ICLR;
		stat_reset = ca->ca_ipl | MC_IRQ_IEN | MC_IRQ_ICLR;
		mcintr_establish(MCV_TIMER1, &sc->sc_profih, "clock");
		mcintr_establish(MCV_TIMER2, &sc->sc_statih, "stat");
		break;
#endif
#if NOFOBIO > 0
	case BUS_OFOBIO:
	    {
		extern struct timecounter ofobio_timecounter; /* below */

		ofobio_timecounter.tc_priv = sc;
		intr_establish(OFOBIOVEC_CLOCK, &sc->sc_profih, "clock");
	    }
		break;
#endif
#if NPCC > 0
	case BUS_PCC:
		prof_reset = ca->ca_ipl | PCC_IRQ_IEN | PCC_TIMERACK;
		stat_reset = ca->ca_ipl | PCC_IRQ_IEN | PCC_TIMERACK;
		pccintr_establish(PCCV_TIMER1, &sc->sc_profih, "clock");
		pccintr_establish(PCCV_TIMER2, &sc->sc_statih, "stat");
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		prof_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		stat_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
		pcctwointr_establish(PCC2V_TIMER1, &sc->sc_profih, "clock");
		pcctwointr_establish(PCC2V_TIMER2, &sc->sc_statih, "stat");
		break;
#endif
	}

	printf("\n");
}

#if NLRC > 0
u_int	lrc_get_timecount(struct timecounter *);
struct timecounter lrc_timecounter = {
	.tc_get_timecount = lrc_get_timecount,
	.tc_counter_mask = 0x00ffffff,	/* 24-bit timer */
	.tc_frequency = 1000000,	/* 1MHz */
	.tc_name = "lrc",
	.tc_quality = 100
};

u_int
lrc_get_timecount(struct timecounter *tc)
{
	/*
	 * Because LRC timers start counting at one, we need to count
	 * wraparounds, and subtract the number of wraparounds, since
	 * we go from FFFFFF to 000001.
	 */
	static u_int lrc_wraparounds = 0;
	static uint32_t lrc_last = 0;
	uint32_t lrc_cur;

	lrc_cur = sys_lrc->lrc_t1cnt & 0x00ffffff;
	if (lrc_cur < lrc_last)
		lrc_wraparounds++;
	lrc_last = lrc_cur;

	return lrc_cur - lrc_wraparounds;
}
#endif

#if NMC > 0
u_int	mc_get_timecount(struct timecounter *);
struct timecounter mc_timecounter = {
	.tc_get_timecount = mc_get_timecount,
	.tc_counter_mask = 0xffffffff,	/* 32-bit timer */
	.tc_frequency = 1000000,	/* 1MHz */
	.tc_name = "mc",
	.tc_quality = 100
};

u_int
mc_get_timecount(struct timecounter *tc)
{
	/*
	 * Note that, theoretically, we ought to check for counter overflows.
	 * However, this timer is free-running with a rollover period of 71.6
	 * minutes - if two time counter readings are separated by more than
	 * this amount, there are more important issues to take care of first.
	 */
	return sys_mc->mc_t3count;
}
#endif

#if NOFOBIO > 0
u_int	ofobio_get_timecount(struct timecounter *);
struct timecounter ofobio_timecounter = {
	.tc_get_timecount = ofobio_get_timecount,
	.tc_counter_mask = 0xffffffff,
	/* .tc_frequency will be filled in */
	.tc_name = "ofobio",
	.tc_quality = 0
};

u_int
ofobio_get_timecount(struct timecounter *tc)
{
	struct clocksoftc *sc = tc->tc_priv;

	/*
	 * XXX It should be possible to get a better resolution by
	 * XXX peeking at the DART internal state, but the way this
	 * XXX clock is wired is horrible enough as is.
	 */
	return sc->sc_profih.ih_count.ec_count;
}
#endif

#if NPCC > 0
u_int	pcc_get_timecount(struct timecounter *);
uint32_t pcc_curcnt;
struct timecounter pcc_timecounter = {
	.tc_get_timecount = pcc_get_timecount,
	.tc_counter_mask = 0xffffffff,	/* 32-bit timer */
	.tc_frequency = PCC_TIMERFREQ,
	.tc_name = "pcc",
	.tc_quality = 100
};

u_int
pcc_get_timecount(struct timecounter *tc)
{
	uint16_t tcr1, tcr2, pload;
	uint8_t tctl;
	u_int cnt;
	int s;

	pload = sys_pcc->pcc_t1pload;
	s = splclock();
	tcr1 = sys_pcc->pcc_t1count;
	tctl = sys_pcc->pcc_t1ctl;
	/*
	 * Since we can not freeze the counter while reading the count
	 * and overflow registers, read them a second time; if the counter
	 * has wrapped, pick the second reading.
	 */
	tcr2 = sys_pcc->pcc_t1count;
	if (tcr2 < tcr1) {
		tcr1 = tcr2;
		tctl = sys_pcc->pcc_t1ctl;
	}
	cnt = pcc_curcnt;
	splx(s);
	tctl >>= PCC_TIMER_OVF_SHIFT;
	while (tctl-- != 0)
		cnt += 0x10000U - pload;
	cnt += tcr1 - pload;

	return cnt;
}
#endif

#if NPCCTWO > 0
u_int	pcctwo_get_timecount(struct timecounter *);
uint32_t pcctwo_curcnt;
struct timecounter pcctwo_timecounter = {
	.tc_get_timecount = pcctwo_get_timecount,
	.tc_counter_mask = 0xffffffff,	/* 32-bit timer */
	.tc_frequency = 1000000,	/* 1MHz */
	.tc_name = "pcctwo",
	.tc_quality = 100
};

u_int
pcctwo_get_timecount(struct timecounter *tc)
{
	uint32_t tcr1, tcr2;
	uint8_t tctl;
	u_int cnt, cmp;
	int s;

	cmp = sys_pcc2->pcc2_t1cmp;
	s = splclock();
	tcr1 = sys_pcc2->pcc2_t1count;
	tctl = sys_pcc2->pcc2_t1ctl;
	/*
	 * Since we can not freeze the counter while reading the count
	 * and overflow registers, read them a second time; if the counter
	 * has wrapped, pick the second reading.
	 */
	tcr2 = sys_pcc2->pcc2_t1count;
	if (tcr2 < tcr1) {
		tcr1 = tcr2;
		tctl = sys_pcc2->pcc2_t1ctl;
	}
	cnt = pcctwo_curcnt;
	splx(s);
	tctl >>= PCC2_TCTL_OVF_SHIFT;
	while (tctl-- != 0)
		cnt += cmp;
	cnt += tcr1;

	return cnt;
}
#endif

/*
 * clockintr: ack intr and call hardclock
 */
int
clockintr(arg)
	void *arg;
{
	u_int oflow = 1;
#if NMC > 0 || NPCC > 0 || NPCCTWO > 0
	uint32_t t1, t2;
	uint8_t c;
#endif

	switch (clockbus) {
#if NLRC > 0
	case BUS_LRC:
		/* nothing to do */
		break;
#endif
#if NOFOBIO > 0
	case BUS_OFOBIO:
		sys_ofobio->csr_c &= ~OFO_CSRC_TIMER_ACK;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		/*
		 * Since we can not freeze the counter while reading the count
		 * and overflow registers, read them a second time; if the
		 * counter has wrapped, pick the second reading.
		 */
		t1 = sys_mc->mc_t1count;
		c = sys_mc->mc_t1ctl;
		t2 = sys_mc->mc_t1count;
		if (t2 < t1)	/* just wrapped */
			c = sys_mc->mc_t1ctl;
		sys_mc->mc_t1ctl = MC_TCTL_CEN | MC_TCTL_COC | MC_TCTL_COVF;
		sys_mc->mc_t1irq = prof_reset;
		oflow = c >> MC_TCTL_OVF_SHIFT;
		break;
#endif
#if NPCC > 0
	case BUS_PCC:
		/*
		 * Since we can not freeze the counter while reading the count
		 * and overflow registers, read them a second time; if the
		 * counter has wrapped, pick the second reading.
		 */
		t1 = sys_pcc->pcc_t1count;
		c = sys_pcc->pcc_t1ctl;
		t2 = sys_pcc->pcc_t1count;
		if (t2 < t1)	/* just wrapped */
			c = sys_pcc->pcc_t1ctl;
		sys_pcc->pcc_t1ctl = PCC_TIMER_COVF | PCC_TIMERSTART;
		sys_pcc->pcc_t1irq = prof_reset;
		oflow = c >> PCC_TIMER_OVF_SHIFT;
		pcc_curcnt += oflow * (0x10000 - sys_pcc->pcc_t1pload);
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		/*
		 * Since we can not freeze the counter while reading the count
		 * and overflow registers, read them a second time; if the
		 * counter has wrapped, pick the second reading.
		 */
		t1 = sys_pcc2->pcc2_t1count;
		c = sys_pcc2->pcc2_t1ctl;
		t2 = sys_pcc2->pcc2_t1count;
		if (t2 < t1)	/* just wrapped */
			c = sys_pcc2->pcc2_t1ctl;
		sys_pcc2->pcc2_t1ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
		    PCC2_TCTL_COVF;
		sys_pcc2->pcc2_t1irq = prof_reset;
		oflow = c >> PCC2_TCTL_OVF_SHIFT;
		pcctwo_curcnt += oflow * sys_pcc2->pcc2_t1cmp;
		break;
#endif
	}

	while (oflow-- != 0)
		hardclock(arg);

	return (1);
}

/*
 * Set up real-time and, if available, statistics clock.
 */
void
cpu_initclocks()
{
	register int statint, minint;

	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
		tick = 1000000 / hz;
	}
	if (stathz == 0)
		stathz = hz;
	if (1000000 % stathz) {
		printf("cannot get %d Hz statclock; using 100 Hz\n", stathz);
		stathz = 100;
	}
	profhz = stathz;		/* always */

	statint = 1000000 / stathz;
	minint = statint / 2 + 100;
	while (statvar > minint)
		statvar >>= 1;
	switch (clockbus) {
#if NLRC > 0
	case BUS_LRC:
		profhz = stathz = 0;	/* only one timer available for now */

		/*
		 * LRC timer usage:
		 * timer0 is used for delay().
		 * timer1 is used for time counters.
		 * timer2 is used for the scheduling clock.
		 */
		sys_lrc->lrc_tcr0 = 0;
		sys_lrc->lrc_tcr1 = TCR_TLD1;	/* reset to one */
		sys_lrc->lrc_tcr1 = TCR_TEN1 | TCR_TCYC1 | TCR_TIS_NONE;

		sys_lrc->lrc_t2base = tick + 1;
		sys_lrc->lrc_tcr2 = TCR_TLD2;	/* reset to one */
		sys_lrc->lrc_tcr2 = TCR_TEN2 | TCR_TCYC2 | TCR_T2IE;

		tc_init(&lrc_timecounter);
		break;
#endif
#if NOFOBIO > 0
	case BUS_OFOBIO:
		profhz = stathz = 0;	/* only one timer available */

		ofobio_clocksetup();

		ofobio_timecounter.tc_frequency = hz;
		tc_init(&ofobio_timecounter);
		break;
#endif
#if NMC > 0
	case BUS_MC:
		/*
		 * MC timer usage:
		 * timer1 is used for the scheduling clock.
		 * timer2 is used for the statistics clock.
		 * timer3 is used for time counters.
		 */

		sys_mc->mc_t1ctl = 0;
		sys_mc->mc_t1cmp = mc_timer_us2lim(tick);
		sys_mc->mc_t1count = 0;
		sys_mc->mc_t1ctl = MC_TCTL_CEN | MC_TCTL_COC | MC_TCTL_COVF;
		sys_mc->mc_t1irq = prof_reset;

		sys_mc->mc_t2ctl = 0;
		sys_mc->mc_t2cmp = mc_timer_us2lim(statint);
		sys_mc->mc_t2count = 0;
		sys_mc->mc_t2ctl = MC_TCTL_CEN | MC_TCTL_COC | MC_TCTL_COVF;
		sys_mc->mc_t2irq = stat_reset;

		sys_mc->mc_t3ctl = 0;
		sys_mc->mc_t3cmp = 0;
		sys_mc->mc_t3count = 0;
		sys_mc->mc_t3ctl = MC_TCTL_CEN;
		sys_mc->mc_t3irq = 0;
		tc_init(&mc_timecounter);
		break;
#endif
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t1pload = pcc_timer_us2lim(tick);
		sys_pcc->pcc_t1ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t1ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t1irq = prof_reset;

		sys_pcc->pcc_t2pload = pcc_timer_us2lim(statint);
		sys_pcc->pcc_t2ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t2ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t2irq = stat_reset;

		tc_init(&pcc_timecounter);
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		/* profclock */
		sys_pcc2->pcc2_t1ctl = 0;
		sys_pcc2->pcc2_t1cmp = pcc2_timer_us2lim(tick);
		sys_pcc2->pcc2_t1count = 0;
		sys_pcc2->pcc2_t1ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
		    PCC2_TCTL_COVF;
		sys_pcc2->pcc2_t1irq = prof_reset;

		/* statclock */
		sys_pcc2->pcc2_t2ctl = 0;
		sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(statint);
		sys_pcc2->pcc2_t2count = 0;
		sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC |
		    PCC2_TCTL_COVF;
		sys_pcc2->pcc2_t2irq = stat_reset;

		tc_init(&pcctwo_timecounter);
		break;
#endif
	}
	statmin = statint - (statvar >> 1);
}

void
setstatclockrate(newhz)
	int newhz;
{
}

int
statintr(cap)
	void *cap;
{
	register u_long newint, r, var;

	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t2irq = stat_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		sys_mc->mc_t2irq = stat_reset;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sys_pcc2->pcc2_t2irq = stat_reset;
		break;
#endif
	}

	statclock((struct clockframe *)cap);

	/*
	 * Compute new randomized interval.  The intervals are uniformly
	 * distributed on [statint - statvar / 2, statint + statvar / 2],
	 * and therefore have mean statint, giving a stathz frequency clock.
	 */
	var = statvar;
	do {
		r = random() & (var - 1);
	} while (r == 0);
	newint = statmin + r;

	switch (clockbus) {
#if NPCC > 0
	case BUS_PCC:
		sys_pcc->pcc_t2pload = pcc_timer_us2lim(newint);
		sys_pcc->pcc_t2ctl = PCC_TIMERCLEAR;
		sys_pcc->pcc_t2ctl = PCC_TIMERSTART;
		sys_pcc->pcc_t2irq = stat_reset;
		break;
#endif
#if NMC > 0
	case BUS_MC:
		sys_mc->mc_t2ctl = 0;
		sys_mc->mc_t2cmp = mc_timer_us2lim(newint);
		sys_mc->mc_t2count = 0;		/* should I? */
		sys_mc->mc_t2irq = stat_reset;
		sys_mc->mc_t2ctl = MC_TCTL_CEN | MC_TCTL_COC;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		sys_pcc2->pcc2_t2ctl = 0;
		sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(newint);
		sys_pcc2->pcc2_t2count = 0;		/* should I? */
		sys_pcc2->pcc2_t2irq = stat_reset;
		sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC;
		break;
#endif
	}
	return (1);
}

void
delay(us)
	int us;
{
#if NPCC > 0 || NOFOBIO > 0
	volatile register int c;
#endif

	switch (clockbus) {
#if NLRC > 0
	case BUS_LRC:
	{
		struct lrcreg *lrc;

		if (sys_lrc != NULL)
			lrc = sys_lrc;
		else
			lrc = (struct lrcreg *)IIOV(0xfff90000);

		/* use timer0 and wait for it to wrap */
		lrc->lrc_t0base = us + 1;
		lrc->lrc_tcr0 = TCR_TLD0;	/* reset to one */
		lrc->lrc_stat = STAT_TMR0;	/* clear latch */
		lrc->lrc_tcr0 = TCR_TEN0;
		while ((lrc->lrc_stat & STAT_TMR0) == 0)
			;
	}
		break;
#endif
#if NMC > 0
	case BUS_MC:
		/*
		 * Reset and restart a free-running timer 1MHz, watch
		 * for it to reach the required count.
		 */
	{
		struct mcreg *mc;

		if (sys_mc != NULL)
			mc = sys_mc;
		else
			mc = (struct mcreg *)IIOV(0xfff00000);

		mc->mc_t3irq = 0;
		mc->mc_t3ctl = 0;
		mc->mc_t3count = 0;
		mc->mc_t3ctl = MC_TCTL_CEN | MC_TCTL_COVF;

		while (mc->mc_t3count < us)
			;
	}
		break;
#endif
#if NPCC > 0 || NOFOBIO > 0
	case BUS_PCC:
	case BUS_OFOBIO:
		/*
		 * XXX MVME147 doesn't have a 3rd free-running timer,
		 * so we use a stupid loop. Fix the code to watch t1:
		 * the profiling timer.
		 * MVME141 only has one timer, so there is no hope
		 * either.
		 */
		c = 2 * us;
		while (--c > 0)
			;
		break;
#endif
#if NPCCTWO > 0
	case BUS_PCCTWO:
		/*
		 * Use the first VMEChip2 timer in polling mode whenever
		 * possible.
		 */
	{
		struct vme2reg *vme2;

		if (sys_vme2 != NULL)
			vme2 = sys_vme2;
		else
			vme2 = (struct vme2reg *)IIOV(0xfff40000);

		vme2->vme2_t1cmp = 0xffffffff;
		vme2->vme2_t1count = 0;
		vme2->vme2_tctl |= VME2_TCTL_CEN;

		while (vme2->vme2_t1count < us)
			;

		vme2->vme2_tctl &= ~VME2_TCTL_CEN;
	}
		break;
#endif
	}
}

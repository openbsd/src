/*	$NetBSD: clock.c,v 1.22 1995/05/29 23:57:15 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1995 Nivas Madhur
 * Copyright (c) 1994 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)clock.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Clock driver. Has both interval timer as well as statistics timer.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#ifdef GPROF
#include <sys/gmon.h>
#endif

#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme88k/dev/pcctworeg.h>

/*
 * Statistics clock interval and variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 4096 would
 * give us offsets in [0..4095].  Instead, we take offsets in [1..4095].
 * This is symmetric about the point 2048, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
int statvar = 8192;
int statmin;			/* statclock interval - 1/2*variance */
int timerok;

u_long delay_factor = 1;

static int	clockmatch __P((struct device *, void *, void *));
static void	clockattach __P((struct device *, struct device *, void *));
int		clockintr __P((void *, void *));
int		statintr __P((void *, void *));

struct clocksoftc {
	struct device			sc_dev;
	volatile struct pcc2reg		*sc_pcc2reg;
};

struct cfattach clock_ca = {
        sizeof(struct clocksoftc), clockmatch, clockattach
}; 
 
struct cfdriver clock_cd = { 
        NULL, "clock", DV_DULL, 0
}; 

struct intrhand clockintrhand, statintrhand;

static int
clockmatch(struct device *parent, void *self, void *aux)
{
	register struct confargs *ca = aux;
	register struct cfdata *cf = self;

	if (ca->ca_bustype != BUS_PCCTWO ||
		strcmp(cf->cf_driver->cd_name, "clock")) {
		return (0);
	}

	/*
	 * clock has to be at ipl 5
	 * We return the ipl here so that the parent can print
	 * a message if it is different from what ioconf.c says.
	 */
	ca->ca_ipl   = IPL_CLOCK;
	/* set size to 0 - see pcctwo.c:match for details */
	ca->ca_size  = 0;

	return 1;
}

/* ARGSUSED */
static void
clockattach(struct device *parent, struct device *self, void *aux)
{
	struct confargs *ca = aux;
	struct clocksoftc *sc = (struct clocksoftc *)self;
	u_long	elapsedtime;

	extern void delay(u_long);
	extern int cpuspeed;

	/*
	 * save virtual address of the pcc2 block since our
	 * registers are in that block.
	 */
	sc->sc_pcc2reg = (struct pcc2reg *)ca->ca_vaddr;

	/*
	 * calibrate for delay() calls.
	 * We do this by using tick timer1 in free running mode before
	 * cpu_initclocks() is called so turn on clock interrupts etc.
	 * 
	 *	the approach is:
	 *		set count in timer to 0
	 *		call delay(1000) for a 1000 us delay
	 *		after return, stop count and figure out
	 *			how many us went by (call it x)
	 *		now the factor to multiply the arg. passed to
	 *		delay would be (x/1000) rounded up to an int.
	 */
	printf("\n");
	sc->sc_pcc2reg->pcc2_t1ctl 	&= ~PCC2_TICTL_CEN;
	sc->sc_pcc2reg->pcc2_psclkadj 	= 256 - cpuspeed;
	sc->sc_pcc2reg->pcc2_t1irq	&= ~PCC2_TTIRQ_IEN;
	sc->sc_pcc2reg->pcc2_t1cntr	= 0;
	sc->sc_pcc2reg->pcc2_t1ctl 	|= PCC2_TICTL_CEN;
	delay(1000);	/* delay for 1 ms */
	sc->sc_pcc2reg->pcc2_t1ctl 	&= ~PCC2_TICTL_CEN;
	elapsedtime = sc->sc_pcc2reg->pcc2_t1cntr;

	delay_factor = (u_long)(elapsedtime / 1000 + 1);

	/*
	 * program clock to interrupt at IPL_CLOCK. Set everything
	 * except compare registers, interrupt enable and counter
	 * enable registers.
	 */
	sc->sc_pcc2reg->pcc2_t1ctl &= ~(PCC2_TICTL_CEN);
	sc->sc_pcc2reg->pcc2_t1cntr= 0;
	sc->sc_pcc2reg->pcc2_t1ctl |= (PCC2_TICTL_COC|PCC2_TICTL_COVF);
	sc->sc_pcc2reg->pcc2_t1irq = (PCC2_TTIRQ_ICLR|IPL_CLOCK);

	sc->sc_pcc2reg->pcc2_t2ctl &= ~(PCC2_TICTL_CEN);
	sc->sc_pcc2reg->pcc2_t2cntr= 0;
	sc->sc_pcc2reg->pcc2_t2ctl |= (PCC2_TICTL_COC|PCC2_TICTL_COVF);
	sc->sc_pcc2reg->pcc2_t2irq = (PCC2_TTIRQ_ICLR|IPL_CLOCK);

	/*
	 * Establish inerrupt handlers.
	 */
	clockintrhand.ih_fn = clockintr;
	clockintrhand.ih_arg = 0; /* don't want anything */
	clockintrhand.ih_ipl = IPL_CLOCK;
	clockintrhand.ih_wantframe = 1;
	intr_establish(PCC2_VECT+9, &clockintrhand);

	statintrhand.ih_fn = statintr;
	statintrhand.ih_arg = 0; /* don't want anything */
	statintrhand.ih_ipl = IPL_CLOCK;
	statintrhand.ih_wantframe = 1;
	intr_establish(PCC2_VECT+8, &statintrhand);

	timerok = 1;
}

/*
 * Set up the real-time and statistics clocks.  Leave stathz 0 only if
 * no alternative timer is available. mvme167/mvme187 has 2 tick timers
 * in pcc2 - we are using timer 1 for clock interrupt and timer 2 for
 * statistics.
 *
 * The frequencies of these clocks must be an even number of microseconds.
 */
cpu_initclocks()
{
	register int statint, minint;
	volatile struct pcc2reg *pcc2reg;

	pcc2reg = ((struct clocksoftc *)clock_cd.cd_devs[0])->sc_pcc2reg;

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
	/*
	 * hz value 100 means we want the clock to interrupt 100
	 * times a sec or 100 times in 1000000 us ie, 1 interrupt
	 * every 10000 us. Program the tick timer compare register
	 * to this value.
	 */
	pcc2reg->pcc2_t1cmp = tick;
	pcc2reg->pcc2_t2cmp = statint;
	statmin = statint - (statvar >> 1);

	/* start the clocks ticking */
	pcc2reg->pcc2_t1ctl = (PCC2_TICTL_CEN|PCC2_TICTL_COC|PCC2_TICTL_COVF);
	pcc2reg->pcc2_t2ctl = (PCC2_TICTL_CEN|PCC2_TICTL_COC|PCC2_TICTL_COVF);
	/* and enable those interrupts */
	pcc2reg->pcc2_t1irq |= (PCC2_TTIRQ_IEN|PCC2_TTIRQ_ICLR);
	pcc2reg->pcc2_t2irq |= (PCC2_TTIRQ_IEN|PCC2_TTIRQ_ICLR);
}

/*
 * Dummy setstatclockrate(), since we know profhz==hz.
 */
/* ARGSUSED */
void
setstatclockrate(int newhz)
{
	/* nothing */
}

/*
 * Delay: wait for `about' n microseconds to pass.
 */
void
delay(volatile u_long n)
{
	volatile u_long cnt = n * delay_factor;

	while (cnt-- > 0) {
		asm volatile("");
	}
}

/*
 * Clock interrupt handler. Calls hardclock() after setting up a
 * clockframe.
 */
int
clockintr(void *cap, void *frame)
{
	volatile struct pcc2reg *reg;

	reg = ((struct clocksoftc *)clock_cd.cd_devs[0])->sc_pcc2reg;
	
	/* Clear the interrupt */
	reg->pcc2_t1irq = (PCC2_TTIRQ_IEN|PCC2_TTIRQ_ICLR|IPL_CLOCK);
#if 0
	reg->pcc2_t1irq |= PCC2_TTIRQ_ICLR;
#endif /* 0 */

	hardclock((struct clockframe *)frame);
#include "bugtty.h"
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */

	return (1);
}

/*
 * Stat clock interrupt handler.
 */
int
statintr(void *cap, void *frame)
{
	volatile struct pcc2reg *reg;
	register u_long newint, r, var;

	reg = ((struct clocksoftc *)clock_cd.cd_devs[0])->sc_pcc2reg;
	
	/* Clear the interrupt */
#if 0
	reg->pcc2_t2irq |= PCC2_TTIRQ_ICLR;
#endif /* 0 */
	reg->pcc2_t2irq = (PCC2_TTIRQ_IEN|PCC2_TTIRQ_ICLR|IPL_CLOCK);

	statclock((struct clockframe *)frame);

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

	/*
	 * reprogram statistics timer to interrupt at
	 * newint us intervals.
	 */
	reg->pcc2_t2ctl = ~(PCC2_TICTL_CEN);
	reg->pcc2_t2cntr = 0;
	reg->pcc2_t2cmp = newint;
	reg->pcc2_t2ctl = (PCC2_TICTL_CEN|PCC2_TICTL_COC|PCC2_TICTL_COVF);
	reg->pcc2_t2irq |= (PCC2_TTIRQ_ICLR|PCC2_TTIRQ_IEN);

	return (1);
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt.
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for
 * fun, we guarantee that the time will be greater than the value
 * obtained by a previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s;
	static struct timeval lasttime;

	s = splhigh();
	*tvp = time;
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

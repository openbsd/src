/*	$OpenBSD: clock.c,v 1.4 1998/12/15 05:52:29 smurph Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>

#include <mvme88k/dev/pcctworeg.h>
#include "pcctwo.h"

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
/*int		clockintr __P((void *, void *));*/
/*#int		statintr __P((void *, void *));*/

struct clocksoftc {
	struct device			sc_dev;
	struct intrhand sc_profih;
	struct intrhand sc_statih;
};

struct cfattach clock_ca = {
        sizeof(struct clocksoftc), clockmatch, clockattach
}; 
 
struct cfdriver clock_cd = { 
        NULL, "clock", DV_DULL, 0
}; 

int	clockintr __P((void *));
int	statintr __P((void *));

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
	register struct confargs *ca = args;
	register struct cfdata *cf = vcf;

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
	ca->ca_len  = 0;


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
	prof_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
	stat_reset = ca->ca_ipl | PCC2_IRQ_IEN | PCC2_IRQ_ICLR;
	pcctwointr_establish(PCC2V_TIMER1, &sc->sc_profih);
	pcctwointr_establish(PCC2V_TIMER2, &sc->sc_statih);

	printf("\n");
}

	/*
 * clockintr: ack intr and call hardclock
	 */
int
clockintr(arg)
	void *arg;
{
	sys_pcc2->pcc2_t1irq = prof_reset;
	hardclock(arg);
#include "bugtty.h"
#if NBUGTTY > 0
	bugtty_chkinput();
#endif /* NBUGTTY */
	return (1);
}

/*
 * Set up real-time clock; we don't have a statistics clock at
 * present.
 */
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

	sys_pcc2->pcc2_t2irq = stat_reset;

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

	sys_pcc2->pcc2_t2ctl = 0;
	sys_pcc2->pcc2_t2cmp = pcc2_timer_us2lim(newint);
	sys_pcc2->pcc2_t2count = 0;		/* should I? */
	sys_pcc2->pcc2_t2irq = stat_reset;
	sys_pcc2->pcc2_t2ctl = PCC2_TCTL_CEN | PCC2_TCTL_COC;

	return (1);
}

delay(us)
	register int us;
{
	volatile register int c;

	/*
	 * XXX MVME167 doesn't have a 3rd free-running timer,
	 * so we use a stupid loop. Fix the code to watch t1:
	 * the profiling timer.
	 */
	c = 4 * us;
	while (--c > 0)
		;
	return (0);
}

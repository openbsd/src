/*	$OpenBSD: sa11x0_ost.c,v 1.4 2005/01/11 21:00:17 deraadt Exp $ */
/*	$NetBSD: sa11x0_ost.c,v 1.11 2003/07/15 00:24:51 lukem Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by IWAMOTO Toshihiro and Ichiro FUKUHARA.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: sa11x0_ost.c,v 1.11 2003/07/15 00:24:51 lukem Exp $");
*/

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <arm/cpufunc.h>

#include <arm/sa11x0/sa11x0_reg.h> 
#include <arm/sa11x0/sa11x0_var.h>
#include <arm/sa11x0/sa11x0_ostreg.h>

static int	saost_match(struct device *, void *, void *);
static void	saost_attach(struct device *, struct device *, void *);

int		gettick(void);
static int	clockintr(void *);
static int	statintr(void *);
void		rtcinit(void);

struct saost_softc {
	struct device		sc_dev;
	bus_addr_t		sc_baseaddr;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	u_int32_t	sc_clock_count;
	u_int32_t	sc_statclock_count;
	u_int32_t	sc_statclock_step;
};

static struct saost_softc *saost_sc = NULL;

#define TIMER_FREQUENCY         3686400         /* 3.6864MHz */
#define TICKS_PER_MICROSECOND   (TIMER_FREQUENCY/1000000)

#ifndef STATHZ
#define STATHZ	64
#endif

#if 0
CFATTACH_DECL(saost, sizeof(struct saost_softc),
    saost_match, saost_attach, NULL, NULL);
#endif

struct cfattach saost_ca = {
	sizeof (struct saost_softc), saost_match, saost_attach
};

struct cfdriver saost_cd = {
	NULL, "saost", DV_DULL
};

static int
saost_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	return (1);
}

void
saost_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct saost_softc *sc = (struct saost_softc*)self;
	struct sa11x0_attach_args *sa = aux;

	printf("\n");

	sc->sc_iot = sa->sa_iot;
	sc->sc_baseaddr = sa->sa_addr;

	saost_sc = sc;

	if(bus_space_map(sa->sa_iot, sa->sa_addr, sa->sa_size, 0, 
			&sc->sc_ioh))
		panic("%s: Cannot map registers", self->dv_xname);

	/* disable all channel and clear interrupt status */
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_IR, 0);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_SR, 0xf);

	printf("%s: SA-11x0 OS Timer\n",  sc->sc_dev.dv_xname);
}

static int
clockintr(arg)
	void *arg;
{
	struct clockframe *frame = arg;
	u_int32_t oscr, nextmatch, oldmatch;
	int s;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_SR, 1);

	/* schedule next clock intr */
	oldmatch = saost_sc->sc_clock_count;
	nextmatch = oldmatch + TIMER_FREQUENCY / hz;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR0,
			  nextmatch);
	oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				SAOST_CR);

	if ((nextmatch > oldmatch &&
	     (oscr > nextmatch || oscr < oldmatch)) ||
	    (nextmatch < oldmatch && oscr > nextmatch && oscr < oldmatch)) {
		/*
		 * we couldn't set the matching register in time.
		 * just set it to some value so that next interrupt happens.
		 * XXX is it possible to compansate lost interrupts?
		 */

		s = splhigh();
		oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
					SAOST_CR);
		nextmatch = oscr + 10;
		bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				  SAOST_MR0, nextmatch);
		splx(s);
	}

	saost_sc->sc_clock_count = nextmatch;
	hardclock(frame);

	return(1);
}

static int
statintr(arg)
	void *arg;
{
	struct clockframe *frame = arg;
	u_int32_t oscr, nextmatch, oldmatch;
	int s;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_SR, 2);

	/* schedule next clock intr */
	oldmatch = saost_sc->sc_statclock_count;
	nextmatch = oldmatch + saost_sc->sc_statclock_step;

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR1,
			  nextmatch);
	oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				SAOST_CR);

	if ((nextmatch > oldmatch &&
	     (oscr > nextmatch || oscr < oldmatch)) ||
	    (nextmatch < oldmatch && oscr > nextmatch && oscr < oldmatch)) {
		/*
		 * we couldn't set the matching register in time.
		 * just set it to some value so that next interrupt happens.
		 * XXX is it possible to compansate lost interrupts?
		 */

		s = splhigh();
		oscr = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
					SAOST_CR);
		nextmatch = oscr + 10;
		bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
				  SAOST_MR1, nextmatch);
		splx(s);
	}

	saost_sc->sc_statclock_count = nextmatch;
	statclock(frame);

	return(1);
}


void
setstatclockrate(hz)
	int hz;
{
	u_int32_t count;

	saost_sc->sc_statclock_step = TIMER_FREQUENCY / hz;
	count = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_CR);
	count += saost_sc->sc_statclock_step;
	saost_sc->sc_statclock_count = count;
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_MR1, count);
}

void
cpu_initclocks()
{
	stathz = STATHZ;
	profhz = stathz;
	saost_sc->sc_statclock_step = TIMER_FREQUENCY / stathz;

	printf("clock: hz=%d stathz=%d\n", hz, stathz);

	/* Use the channels 0 and 1 for hardclock and statclock, respectively */
	saost_sc->sc_clock_count = TIMER_FREQUENCY / hz;
	saost_sc->sc_statclock_count = TIMER_FREQUENCY / stathz;

	sa11x0_intr_establish(0, 26, 1, IPL_CLOCK, clockintr, 0, "clock");
	sa11x0_intr_establish(0, 27, 1, IPL_CLOCK, statintr, 0, "stat");

	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_SR, 0xf);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_IR, 3);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR0,
			  saost_sc->sc_clock_count);
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_MR1,
			  saost_sc->sc_statclock_count);

	/* Zero the counter value */
	bus_space_write_4(saost_sc->sc_iot, saost_sc->sc_ioh, SAOST_CR, 0);
}

int
gettick()
{
	int counter;
	u_int savedints;
	savedints = disable_interrupts(I32_bit);

	counter = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_CR);

	restore_interrupts(savedints);
	return counter;
}

void
microtime(tvp)
	register struct timeval *tvp;
{
	int s, tm, deltatm;
	static struct timeval lasttime;

	if(saost_sc == NULL) {
		tvp->tv_sec = 0;
		tvp->tv_usec = 0;
		return;
	}

	s = splhigh();
	tm = bus_space_read_4(saost_sc->sc_iot, saost_sc->sc_ioh,
			SAOST_CR);

	deltatm = saost_sc->sc_clock_count - tm;

#ifdef OST_DEBUG
	printf("deltatm = %d\n",deltatm);
#endif

	*tvp = time;
	tvp->tv_usec++;		/* XXX */
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
		tvp->tv_usec <= lasttime.tv_usec &&
		(tvp->tv_usec = lasttime.tv_usec + 1) >= 1000000)
	{
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

void
delay(usecs)
	u_int usecs;
{
	u_int32_t tick, otick, delta;
	int j, csec, usec;

	csec = usecs / 10000;
	usec = usecs % 10000;
	
	usecs = (TIMER_FREQUENCY / 100) * csec
	    + (TIMER_FREQUENCY / 100) * usec / 10000;

	if (! saost_sc) {
		/* clock isn't initialized yet */
		for(; usecs > 0; usecs--)
			for(j = 100; j > 0; j--)
				;
		return;
	}

	otick = gettick();

	while (1) {
		for(j = 100; j > 0; j--)
			;
		tick = gettick();
		delta = tick - otick;
		if (delta > usecs)
			break;
		usecs -= delta;
		otick = tick;
	}
}

void
resettodr()
{
}

void
inittodr(base)
	time_t base;
{
	time.tv_sec = base;
	time.tv_usec = 0;
}

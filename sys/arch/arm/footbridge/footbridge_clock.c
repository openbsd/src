/*	$OpenBSD: footbridge_clock.c,v 1.6 2004/08/18 13:25:26 drahn Exp $	*/
/*	$NetBSD: footbridge_clock.c,v 1.17 2003/03/23 14:12:25 chris Exp $	*/

/*
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited.
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

/* Include header files */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/device.h>

#include <machine/intr.h>

#include <arm/cpufunc.h>

#include <arm/footbridge/dc21285reg.h>
#include <arm/footbridge/footbridgevar.h>
#include <arm/footbridge/footbridge.h>

extern struct footbridge_softc *clock_sc;
extern u_int dc21285_fclk;

int clockhandler (void *);
int statclockhandler (void *);
static int load_timer (int, int);

/*
 * Statistics clock variance, in usec.  Variance must be a
 * power of two.  Since this gives us an even number, not an odd number,
 * we discard one case and compensate.  That is, a variance of 1024 would
 * give us offsets in [0..1023].  Instead, we take offsets in [1..1023].
 * This is symmetric about the point 512, or statvar/2, and thus averages
 * to that value (assuming uniform random numbers).
 */
const int statvar = 1024;
int statmin;			/* minimum stat clock count in ticks */
int statcountperusec;		/* number of ticks per usec at current stathz */
int statprev;			/* last value of we set statclock to */

#if 0
static int clockmatch	(struct device *parent, struct cfdata *cf, void *aux);
static void clockattach	(struct device *parent, struct device *self, void *aux);

CFATTACH_DECL(footbridge_clock, sizeof(struct clock_softc),
    clockmatch, clockattach, NULL, NULL);

/*
 * int clockmatch(struct device *parent, void *match, void *aux)
 *
 * Just return ok for this if it is device 0
 */ 
 
static int
clockmatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	union footbridge_attach_args *fba = aux;

	if (strcmp(fba->fba_ca.ca_name, "clk") == 0)
		return(1);
	return(0);
}


/*
 * void clockattach(struct device *parent, struct device *dev, void *aux)
 *
 */
  
static void
clockattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct clock_softc *sc = (struct clock_softc *)self;
	union footbridge_attach_args *fba = aux;

	sc->sc_iot = fba->fba_ca.ca_iot;
	sc->sc_ioh = fba->fba_ca.ca_ioh;

	clock_sc = sc;

	/* Cannot do anything until cpu_initclocks() has been called */
	
	printf("\n");
}
#endif

/*
 * int clockhandler(struct clockframe *frame)
 *
 * Function called by timer 1 interrupts.
 * This just clears the interrupt condition and calls hardclock().
 */

int
clockhandler(aframe)
	void *aframe;
{
	struct clockframe *frame = aframe;
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    TIMER_1_CLEAR, 0);
	hardclock(frame);
	{
		void debugled(u_int32_t);
		extern int ticks;
		debugled(ticks);
	}
	return(-1);	/* Pass the interrupt on down the chain */
}

/*
 * int statclockhandler(struct clockframe *frame)
 *
 * Function called by timer 2 interrupts.
 * This just clears the interrupt condition and calls statclock().
 */
 
int
statclockhandler(aframe)
	void *aframe;
{
	struct clockframe *frame = aframe;
	int newint, r;
	int currentclock ;

	/* start the clock off again */
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
			TIMER_2_CLEAR, 0);

	do {
		r = random() & (statvar-1);
	} while (r == 0);
	newint = statmin + (r * statcountperusec);
	
	/* fetch the current count */
	currentclock = bus_space_read_4(clock_sc->sc_iot, clock_sc->sc_ioh,
		    TIMER_2_VALUE);

	/* 
	 * work out how much time has run, add another usec for time spent
	 * here
	 */
	r = ((statprev - currentclock) + statcountperusec);

	if (r < newint) {
		newint -= r;
		r = 0;
	}
	else 
		printf("statclockhandler: Statclock overrun\n");


	/* 
	 * update the clock to the new counter, this reloads the existing
	 * timer
	 */
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    		TIMER_2_LOAD, newint);
	statprev = newint;
	statclock(frame);
	if (r)
		/*
		 * We've completely overrun the previous interval,
		 * make sure we report the correct number of ticks. 
		 */
		statclock(frame);

	return(-1);	/* Pass the interrupt on down the chain */
}

static int
load_timer(base, hz)
	int base;
	int hz;
{
	unsigned int timer_count;
	int control;

	timer_count = dc21285_fclk / hz;
	if (timer_count > TIMER_MAX_VAL * 16) {
		control = TIMER_FCLK_256;
		timer_count >>= 8;
	} else if (timer_count > TIMER_MAX_VAL) {
		control = TIMER_FCLK_16;
		timer_count >>= 4;
	} else
		control = TIMER_FCLK;

	control |= (TIMER_ENABLE | TIMER_MODE_PERIODIC);
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    base + TIMER_LOAD, timer_count);
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    base + TIMER_CONTROL, control);
	bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    base + TIMER_CLEAR, 0);
	return(timer_count);
}

/*
 * void setstatclockrate(int hz)
 *
 * Set the stat clock rate. The stat clock uses timer2
 */

void
setstatclockrate(hz)
	int hz;
{
	int statint;
	int countpersecond;
	int statvarticks;

	/* statint == num in counter to drop by desired hz */
	statint = statprev = clock_sc->sc_statclock_count =
	    load_timer(TIMER_2_BASE, hz);
	
	/* Get the total ticks a second */
	countpersecond = statint * hz;
	
	/* now work out how many ticks per usec */
	statcountperusec = countpersecond / 1000000;

	/* calculate a variance range of statvar */
	statvarticks = statcountperusec * statvar;
	
	/* minimum is statint - 50% of variant */
	statmin = statint - (statvarticks / 2);
}

/*
 * void cpu_initclocks(void)
 *
 * Initialise the clocks.
 *
 * Timer 1 is used for the main system clock (hardclock)
 * Timer 2 is used for the statistics clock (statclock)
 */
 
void
cpu_initclocks()
{
	/* stathz and profhz should be set to something, we have the timer */

	if (stathz == 0)
		stathz = 128;

	if (profhz == 0)
		profhz = stathz * 8;

	/* Report the clock frequencies */
	printf("clock: hz %d stathz %d profhz %d\n", hz, stathz, profhz);

	/* Setup timer 1 and claim interrupt */
	clock_sc->sc_clock_count = load_timer(TIMER_1_BASE, hz);

	/*
	 * Use ticks per 256us for accuracy since ticks per us is often
	 * fractional e.g. @ 66MHz
	 */
	clock_sc->sc_clock_ticks_per_256us =
	    ((((clock_sc->sc_clock_count * hz) / 1000) * 256) / 1000);
	clock_sc->sc_clockintr = footbridge_intr_claim(IRQ_TIMER_1, IPL_CLOCK,
	    "clock", clockhandler, 0);

	if (clock_sc->sc_clockintr == NULL)
		panic("%s: Cannot install timer 1 interrupt handler",
		    clock_sc->sc_dev.dv_xname);

	/* If stathz is non-zero then setup the stat clock */
	if (stathz) {
		/* Setup timer 2 and claim interrupt */
		setstatclockrate(stathz);
       		clock_sc->sc_statclockintr = footbridge_intr_claim(IRQ_TIMER_2, IPL_STATCLOCK,
       		    "stat", statclockhandler, 0);
		if (clock_sc->sc_statclockintr == NULL)
			panic("%s: Cannot install timer 2 interrupt handler",
			    clock_sc->sc_dev.dv_xname);
	}
}


/*
 * void microtime(struct timeval *tvp)
 *
 * Fill in the specified timeval struct with the current time
 * accurate to the microsecond.
 */

void
microtime(tvp)
	struct timeval *tvp;
{
	int s;
	int tm;
	int deltatm;
	static struct timeval oldtv;

	if (clock_sc == NULL || clock_sc->sc_clock_count == 0)
		return;

	s = splhigh();

	tm = bus_space_read_4(clock_sc->sc_iot, clock_sc->sc_ioh,
	    TIMER_1_VALUE);

	deltatm = clock_sc->sc_clock_count - tm;

#ifdef DIAGNOSTIC
	if (deltatm < 0)
		panic("opps deltatm < 0 tm=%d deltatm=%d", tm, deltatm);
#endif

	/* Fill in the timeval struct */
	*tvp = time;
	tvp->tv_usec += ((deltatm << 8) / clock_sc->sc_clock_ticks_per_256us);

	/* Make sure the micro seconds don't overflow. */
	while (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		++tvp->tv_sec;
	}

	/* Make sure the time has advanced. */
	if (tvp->tv_sec == oldtv.tv_sec &&
	    tvp->tv_usec <= oldtv.tv_usec) {
		tvp->tv_usec = oldtv.tv_usec + 1;
		if (tvp->tv_usec >= 1000000) {
			tvp->tv_usec -= 1000000;
			++tvp->tv_sec;
		}
	}

	oldtv = *tvp;
	(void)splx(s);		
}

/*
 * Use a timer to track microseconds, if the footbridge hasn't been setup we
 * rely on an estimated loop, however footbridge is attached very early on.
 */

static int delay_clock_count = 0;
static int delay_count_per_usec = 0;

void
calibrate_delay(void)
{
     delay_clock_count = load_timer(TIMER_3_BASE, 100);
     delay_count_per_usec = delay_clock_count/10000;
#ifdef VERBOSE_DELAY_CALIBRATION
     printf("delay calibration: delay_cc = %d, delay_c/us=%d\n",
		     delay_clock_count, delay_count_per_usec);
     
     printf("0..");
     delay(1000000);
     printf("1..");
     delay(1000000);
     printf("2..");
     delay(1000000);
     printf("3..");
     delay(1000000);
     printf("4..");
      delay(1000000);
     printf("5..");
      delay(1000000);
     printf("6..");
      delay(1000000);
     printf("7..");
      delay(1000000);
     printf("8..");
      delay(1000000);
     printf("9..");
      delay(1000000);
     printf("10\n");
#endif
}

int delaycount = 25000;

void
delay(n)
	u_int n;
{
	volatile u_int i;
	uint32_t cur, last, delta, usecs;

	if (n == 0) return;


	/* 
	 * not calibrated the timer yet, so try to live with this horrible
	 * loop!
	 */
	if (delay_clock_count == 0)
	{
	    while (n-- > 0) {
		for (i = delaycount; --i;);
	    }
	    return;	
	}

	/* 
	 * read the current value (do not reset it as delay is reentrant)
	 */
	last = bus_space_read_4(clock_sc->sc_iot, clock_sc->sc_ioh,
		    TIMER_3_VALUE);
	 
	delta = usecs = 0;

	while (n > usecs)
	{
	    cur = bus_space_read_4(clock_sc->sc_iot, clock_sc->sc_ioh,
		    TIMER_3_VALUE);
	    if (last < cur)
		/* timer has wrapped */
		delta += ((delay_clock_count - cur) + last);
	    else
		delta += (last - cur);
	    
	    if (cur == 0)
	    {
		/*
		 * reset the timer, note that if something blocks us for more
		 * than 1/100s we may delay for too long, but I believe that
		 * is fairly unlikely.
		 */
		bus_space_write_4(clock_sc->sc_iot, clock_sc->sc_ioh,
			TIMER_3_CLEAR, 0);
	    }
	    last = cur;
	    
	    if (delta >= delay_count_per_usec)
	    {
		usecs += delta / delay_count_per_usec;
		delta %= delay_count_per_usec;
	    }
	}
}

/* End of footbridge_clock.c */

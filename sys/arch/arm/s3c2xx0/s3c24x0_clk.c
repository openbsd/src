/*	$OpenBSD: s3c24x0_clk.c,v 1.1 2008/11/26 14:39:14 drahn Exp $ */
/*	$NetBSD: s3c24x0_clk.c,v 1.10 2008/07/04 11:59:45 bsh Exp $ */

/*
 * Copyright (c) 2003  Genetec corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec corporation may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORP. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORP.
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
__KERNEL_RCSID(0, "$NetBSD: s3c24x0_clk.c,v 1.10 2008/07/04 11:59:45 bsh Exp $");
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <arm/cpufunc.h>

#include <arm/s3c2xx0/s3c24x0reg.h>
#include <arm/s3c2xx0/s3c24x0var.h>
#include <arm/s3c2xx0/s3c2xx0reg.h>


#ifndef STATHZ
#define STATHZ	64
#endif

#define TIMER_FREQUENCY(pclk) ((pclk)/16) /* divider=1/16 */

static unsigned int timer4_reload_value;
static unsigned int timer4_prescaler;
static unsigned int timer4_mseccount;

#define usec_to_counter(t)	\
	((timer4_mseccount*(t))/1000)

#define counter_to_usec(c,pclk)	\
	(((c)*timer4_prescaler*1000)/(TIMER_FREQUENCY(pclk)/1000))

static u_int	s3c24x0_get_timecount(struct timecounter *);

static struct timecounter s3c24x0_timecounter = {
	s3c24x0_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask */
	0,		/* frequency */
	"s3c234x0",		/* name */
	100,			/* quality */
	NULL,			/* prev */
	NULL,			/* next */
};

static volatile uint32_t s3c24x0_base;

static u_int
s3c24x0_get_timecount(struct timecounter *tc)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) s3c2xx0_softc;
	int save, int_pend0, int_pend1, count;

	save = disable_interrupts(I32_bit);

 again:
	int_pend0 = S3C24X0_INT_TIMER4 &
	    bus_space_read_4(sc->sc_sx.sc_iot, sc->sc_sx.sc_intctl_ioh,
		INTCTL_SRCPND);
	count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
	    TIMER_TCNTO(4));
	
	for (;;) {

		int_pend1 = S3C24X0_INT_TIMER4 &
		    bus_space_read_4(sc->sc_sx.sc_iot, sc->sc_sx.sc_intctl_ioh,
			INTCTL_SRCPND);
		if( int_pend0 == int_pend1 )
			break;

		/*
		 * Down counter reached to zero while we were reading
		 * timer values. do it again to get consistent values.
		 */
		int_pend0 = int_pend1;
		count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
		    TIMER_TCNTO(4));
	}

	if (__predict_false(count > timer4_reload_value)) {
		/* 
		 * Buggy Hardware Warning --- sometimes timer counter
		 * reads bogus value like 0xffff.  I guess it happens when
		 * the timer is reloaded.
		 */
		printf("Bogus value from timer counter: %d\n", count);
		goto again;
	}

	restore_interrupts(save);

	if (int_pend1)
		count -= timer4_reload_value;

	return s3c24x0_base - count;
}

static inline int
read_timer(struct s3c24x0_softc *sc)
{
	int count;

	do {
		count = bus_space_read_2(sc->sc_sx.sc_iot, sc->sc_timer_ioh,
		    TIMER_TCNTO(4));
	} while ( __predict_false(count > timer4_reload_value) );

	return count;
}

/*
 * delay:
 *
 *	Delay for at least N microseconds.
 */
void
delay(u_int n)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *) s3c2xx0_softc;
	int v0, v1, delta;
	u_int ucnt;

	if ( timer4_reload_value == 0 ){
		/* not initialized yet */
		while ( n-- > 0 ){
			int m;

			for (m=0; m<100; ++m )
				;
		}
		return;
	}

	/* read down counter */
	v0 = read_timer(sc);

	ucnt = usec_to_counter(n);

	while( ucnt > 0 ) {
		v1 = read_timer(sc);
		delta = v0 - v1;
		if ( delta < 0 )
			delta += timer4_reload_value;
#ifdef DEBUG
		if (delta < 0 || delta > timer4_reload_value)
			panic("wrong value from timer counter");
#endif

		if((u_int)delta < ucnt){
			ucnt -= (u_int)delta;
			v0 = v1;
		}
		else {
			ucnt = 0;
		}
	}
	/*NOTREACHED*/
}

void
setstatclockrate(int newhz)
{
}

static int
hardintr(void *arg)
{
	#if 0
	atomic_add_32(&s3c24x0_base, timer4_reload_value);
	#else
	s3c24x0_base += timer4_reload_value;
	#endif

	hardclock((struct clockframe *)arg);

	return 1;
}

void
cpu_initclocks(void)
{
	struct s3c24x0_softc *sc = (struct s3c24x0_softc *)s3c2xx0_softc;
	long tc;
	int prescaler, h;
	int pclk = s3c2xx0_softc->sc_pclk;
	bus_space_tag_t iot = sc->sc_sx.sc_iot;
	bus_space_handle_t ioh = sc->sc_timer_ioh;
	uint32_t  reg;

	stathz = STATHZ;
	profhz = stathz;

#define	time_constant(hz)	(TIMER_FREQUENCY(pclk) /(hz)/ prescaler)
#define calc_time_constant(hz)					\
	do {							\
		prescaler = 1;					\
		do {						\
			++prescaler;				\
			tc = time_constant(hz);			\
		} while( tc > 65536 );				\
	} while(0)


	/* Use the channels 4 and 3 for hardclock and statclock, respectively */

	/* stop all timers */
	bus_space_write_4(iot, ioh, TIMER_TCON, 0);

	/* calc suitable prescaler value */
	h = MIN(hz,stathz);
	calc_time_constant(h);

	timer4_prescaler = prescaler;
	timer4_reload_value = TIMER_FREQUENCY(pclk) / hz / prescaler;
	timer4_mseccount = TIMER_FREQUENCY(pclk)/timer4_prescaler/1000 ;

	bus_space_write_4(iot, ioh, TIMER_TCNTB(4),
	    ((prescaler - 1) << 16) | (timer4_reload_value - 1));

	printf("clock: hz=%d stathz = %d PCLK=%d prescaler=%d tc=%ld\n",
	    hz, stathz, pclk, prescaler, tc);

	bus_space_write_4(iot, ioh, TIMER_TCNTB(3),
	    ((prescaler - 1) << 16) | (time_constant(stathz) - 1));

	s3c24x0_intr_establish(S3C24X0_INT_TIMER4, IPL_CLOCK, 
			       IST_NONE, hardintr, 0);
#if 0
#ifdef IPL_STATCLOCK
	s3c24x0_intr_establish(S3C24X0_INT_TIMER3, IPL_STATCLOCK,
			       IST_NONE, statintr, 0);
#endif
#endif

	/* set prescaler1 */
	reg = bus_space_read_4(iot, ioh, TIMER_TCFG0);
	bus_space_write_4(iot, ioh, TIMER_TCFG0,
			  (reg & ~0xff00) | ((prescaler-1) << 8));

	/* divider 1/16 for ch #3 and #4 */
	reg = bus_space_read_4(iot, ioh, TIMER_TCFG1);
	bus_space_write_4(iot, ioh, TIMER_TCFG1,
			  (reg & ~(TCFG1_MUX_MASK(3)|TCFG1_MUX_MASK(4))) |
			  (TCFG1_MUX_DIV16 << TCFG1_MUX_SHIFT(3)) |
			  (TCFG1_MUX_DIV16 << TCFG1_MUX_SHIFT(4)) );


	/* start timers */
	reg = bus_space_read_4(iot, ioh, TIMER_TCON);
	reg &= ~(TCON_MASK(3)|TCON_MASK(4));

	/* load the time constant */
	bus_space_write_4(iot, ioh, TIMER_TCON, reg |
	    TCON_MANUALUPDATE(3) | TCON_MANUALUPDATE(4));
	/* set auto reload and start */
	bus_space_write_4(iot, ioh, TIMER_TCON, reg |
	    TCON_AUTORELOAD(3) | TCON_START(3) |
	    TCON_AUTORELOAD(4) | TCON_START(4) );

	s3c24x0_timecounter.tc_frequency = TIMER_FREQUENCY(pclk) / timer4_prescaler;
	tc_init(&s3c24x0_timecounter);
}


#if 0
/* test routine for delay() */

void delay_test(void);
void
delay_test(void)
{
	struct s3c2xx0_softc *sc = s3c2xx0_softc;
	volatile int *pdatc = (volatile int *)
		((char *)bus_space_vaddr(sc->sc_iot, sc->sc_gpio_ioh) + GPIO_PDATC);
	static const int d[] = {0, 1, 5, 10, 50, 100, 500, 1000, -1};
	int i;
	int v = *pdatc & ~0x07;

	for (;;) {
		*pdatc = v | 2;

		for (i=0; d[i] >= 0; ++i) {
			*pdatc = v | 3;
			delay(d[i]);
			*pdatc = v | 2;
		}
		*pdatc = v;
	}
}
#endif


void
inittodr(time_t base)  
{
}

void
resettodr(void)
{
}

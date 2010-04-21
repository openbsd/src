/*	$OpenBSD: rtc_clock.c,v 1.1 2010/04/21 19:33:45 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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

/*
 * Clock routines using a freerunning RTC counter (models 530/4600)
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/timetc.h>

#include <machine/board.h>
#include <machine/avcommon.h>
#include <machine/av530.h>

#include <aviion/dev/sysconvar.h>

extern u_int aviion_delay_const;

struct intrhand rtc_clock_ih;

int	rtc_clockintr(void *);
u_int	rtc_get_timecount(struct timecounter *);

struct timecounter rtc_timecounter = {
	.tc_get_timecount = rtc_get_timecount,
	.tc_counter_mask = 0xffffffff,
	.tc_name = "rtc",
	.tc_quality = 0
};

uint32_t pit_step;

void
rtc_init_clocks()
{
	uint rtc_speed, pit_interval;

#ifdef DIAGNOSTIC
	if (1000000 % hz) {
		printf("cannot get %d Hz clock; using 100 Hz\n", hz);
		hz = 100;
	}
#endif
	tick = 1000000 / hz;

	profhz = stathz = 0;

	/*
	 * According to the documentation, systems of this family run
	 * at 33MHz; however other sources seem to imply model 530
	 * runs at 25Mhz only.
	 * If this turns out to be the case, proper calibration of the
	 * system clock will be necessary.
	 */
	aviion_delay_const = 33;
	rtc_speed = 33333333 / 2;	/* apparently RTC runs at half CPUCLK */

	/*
	 * RTC is free running; we can get a periodic interrupt at any
	 * multiple of 0x100 RTC ticks. We use PIT0 for this purpose.
	 * Therefore to get a proper system clock, we need an interrupt
	 * every (rtc_speed / hz) ticks, rounded down.
	 * With hz being 100, this won't be an exact 100Hz clock, but the
	 * timecounter code will make sure time is kept accurately.
	 */

	/* disable and reset all counters */
	*(volatile uint32_t *)AV530_PIT_CMD_ALL = AV530_PIT_RESET;
	/* setup countdown interrupt threshold */
	pit_interval = rtc_speed / hz;
	pit_step = 0xffffff00 & (-pit_interval);
	*(volatile uint32_t *)AV530_PIT0_CNT = pit_step;
	/* start timer */
	*(volatile uint32_t *)AV530_PIT0_CS = AV530_PIT_CTEN;

	rtc_clock_ih.ih_fn = rtc_clockintr;
	rtc_clock_ih.ih_arg = 0;
	rtc_clock_ih.ih_flags = INTR_WANTFRAME;
	rtc_clock_ih.ih_ipl = IPL_CLOCK;
	sysconintr_establish(INTSRC_CLOCK, &rtc_clock_ih, "clock");

	rtc_timecounter.tc_frequency = rtc_speed;
	tc_init(&rtc_timecounter);
}

int
rtc_clockintr(void *frame)
{
	/*
	 * Not only does the PIT stop upon overflow (requiring us to
	 * rearm it after acknowledging the interrupt), but also the
	 * comparison value is lost and needs to be setup again.
	 */
	*(volatile uint32_t *)AV530_PIT0_CS = AV530_PIT_IACK | AV530_PIT_RESET;
	*(volatile uint32_t *)AV530_PIT0_CNT = pit_step;
	*(volatile uint32_t *)AV530_PIT0_CS = AV530_PIT_CTEN;
	hardclock(frame);

	return 1;
}

u_int
rtc_get_timecount(struct timecounter *tc)
{
	return *(volatile uint32_t *)AV530_RTC_CNT;
}
